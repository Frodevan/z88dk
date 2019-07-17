/*
Z88DK Z80 Macro Assembler

Copyright (C) Paulo Custodio, 2011-2017
License: The Artistic License 2.0, http://www.perlfoundation.org/artistic_license_2_0
Repository: https://github.com/pauloscustodio/z88dk-z80asm

Assembly macros.
*/

#include "macros.h"
#include "alloc.h"
#include "errors.h"
#include "str.h"
#include "strutil.h"
#include "uthash.h"
#include "types.h"
#include "die.h"
#include <ctype.h>

#define Is_ident_prefix(x)	((x)=='.' || (x)=='#' || (x)=='$' || (x)=='%' || (x)=='@')
#define Is_ident_start(x)	(isalpha(x) || (x)=='_')
#define Is_ident_cont(x)	(isalnum(x) || (x)=='_')

//-----------------------------------------------------------------------------
//	#define macros
//-----------------------------------------------------------------------------
typedef struct DefMacro
{
	const char	*name;					// string kept in strpool.h
	argv_t		*params;				// list of formal parameters
	str_t		*text;					// replacement text
	bool		 expanding;				// true if inside macro expansion for this macro
	UT_hash_handle hh;      			// hash table
} DefMacro;

static DefMacro *def_macros = NULL;		// global list of #define macros
static DefMacro* last_def_macro = NULL;	// last #define macro defined, used by #defcont
static argv_t *in_lines = NULL;			// line stream from input
static argv_t *out_lines = NULL;		// line stream to ouput
static str_t *current_line = NULL;		// current returned line
static bool in_defgroup;				// no EQU transformation in defgroup
static getline_t cur_getline_func = NULL; // callback to read a line from input

static DefMacro *DefMacro_add(char *name)
{
	DefMacro *elem;
	HASH_FIND(hh, def_macros, name, strlen(name), elem);
	if (elem) 
		return NULL;		// duplicate

	elem = m_new(DefMacro);
	elem->name = spool_add(name);
	elem->params = argv_new();
	elem->text = str_new();
	elem->expanding = false;
	HASH_ADD_KEYPTR(hh, def_macros, elem->name, strlen(name), elem);

	last_def_macro = elem;
	return elem;
}

static void DefMacro_delete_elem(DefMacro *elem)
{
	if (elem) {
		argv_free(elem->params);
		str_free(elem->text);
		HASH_DEL(def_macros, elem);
		m_free(elem);
	}
}

static void DefMacro_delete(char *name)
{
	DefMacro *elem;
	HASH_FIND(hh, def_macros, name, strlen(name), elem);
	DefMacro_delete_elem(elem);		// it is OK to undef a non-existing macro
}

static DefMacro *DefMacro_lookup(char *name)
{
	DefMacro *elem;
	HASH_FIND(hh, def_macros, name, strlen(name), elem);
	return elem;
}

//-----------------------------------------------------------------------------
//	module API
//-----------------------------------------------------------------------------
void init_macros()
{
	def_macros = NULL;
	last_def_macro = NULL;
	in_defgroup = false;
	in_lines = argv_new();
	out_lines = argv_new();
	current_line = str_new();
}

void clear_macros()
{
	DefMacro *elem, *tmp;
	HASH_ITER(hh, def_macros, elem, tmp) {
		DefMacro_delete_elem(elem);
	}
	def_macros = NULL;
	last_def_macro = NULL;
	in_defgroup = false;

	argv_clear(in_lines);
	argv_clear(out_lines);
	str_clear(current_line);
}

void free_macros()
{
	clear_macros();

	argv_free(in_lines);
	argv_free(out_lines);
	str_free(current_line);
}

//-----------------------------------------------------------------------------
//	parser
//-----------------------------------------------------------------------------

// fill input stream
static void fill_input()
{
	if (argv_len(in_lines) == 0) {
		if (cur_getline_func != NULL) {
			char* line = cur_getline_func();
			if (line)
				argv_push(in_lines, line);
		}
	}
}

// extract first line from input_stream to current_line
static bool shift_lines(argv_t *lines)
{
	str_clear(current_line);
	if (argv_len(lines) > 0) {
		// copy first from stream
		char *line = *argv_front(lines);
		str_set(current_line, line);
		argv_shift(lines);
		return true;
	}
	else
		return false;
}

// collect a quoted string from input pointer to output string
static bool collect_quoted_string(char** p, str_t* out)
{
#define P (*p)
	if (*P == '\'' || *P == '"') {
		char q = *P; str_append_n(out, P, 1); P++;
		while (*P != q && *P != '\0') {
			if (*P == '\\') {
				str_append_n(out, P, 1); P++;
				if (*P != '\0') {
					str_append_n(out, P, 1); P++;
				}
			}
			else {
				str_append_n(out, P, 1); P++;
			}
		}
		if (*P != '\0') {
			str_append_n(out, P, 1); P++;
		}
		return true;
	}
	else
		return false;
#undef P
}

// collect a macro or argument name [\.\#]?[a-z_][a-z_0-9]*
static bool collect_name(char **in, str_t *out)
{
	char *p = *in;

	str_clear(out);
	while (isspace(*p)) p++;

	if (Is_ident_prefix(p[0]) && Is_ident_start(p[1])) {
		str_append_n(out, p, 2); p += 2;
		while (Is_ident_cont(*p)) { str_append_n(out, p, 1); p++; }
		*in = p;
		return true;
	}
	else if (Is_ident_start(p[0])) {
		while (Is_ident_cont(*p)) { str_append_n(out, p, 1); p++; }
		*in = p;
		return true;
	}
	else {
		return false;
	}
}

// collect formal parameters
static bool collect_formal_params1(char** p, DefMacro* macro, str_t* param)
{
#define P (*p)
	if (*P == '(') P++; else return true;			// (
	while (isspace(*P)) P++;						// blanks
	if (*P == ')') { P++; return true; }			// )
	
	while (true) {
		if (!collect_name(&P, param)) return false;	// NAME
		argv_push(macro->params, str_data(param));
		while (isspace(*P)) P++;					// blanks
	
		if (*P == ')') { P++; return true; }		// )
		else if (*P == ',') { P++; }				// ,
		else { return false; }
	}
#undef P
}

static bool collect_formal_params(char **p, DefMacro *macro)
{
	str_t* param = str_new();
	bool found = collect_formal_params1(p, macro, param);
	str_free(param);
	return found;
}

// collect macro text
static bool collect_macro_text1(char** p, DefMacro* macro, str_t* text)
{
#define P (*p)
	str_clear(text);
	while (isspace(*P)) P++;
	while (*P) {
		if (*P == ';' || *P == '\n')
			break;
		else if (collect_quoted_string(&P, text)) {
		}
		else if (P[0] == '\\' && P[1] == '\n') {		// continuation line
			str_append_n(text, " ", 1);
			fill_input();
			if (shift_lines(in_lines)) P = str_data(current_line); else P = "";
		}
		else {
			str_append_n(text, P, 1); P++;
		}
	}

	while (str_len(text) > 0 && isspace(str_data(text)[str_len(text) - 1])) {
		str_len(text)--;
		str_data(text)[str_len(text)] = '\0';
	}

	if (str_len(macro->text) > 0)
		str_append_n(macro->text, "\n", 1);
	str_append_str(macro->text, text);

	return true;
#undef P
}

static bool collect_macro_text(char **p, DefMacro *macro)
{
	str_t* text = str_new();
	bool found = collect_macro_text1(p, macro, text);
	str_free(text);
	return found;
}

// collect white space up to end of line or comment
static bool collect_eol(char **p)
{
#define P (*p)

	while (isspace(*P)) P++; // consumes also \n and \r
	if (*P == ';' || *P == '\0')
		return true;
	else
		return false;

#undef P
}

// is this an identifier?
static bool collect_ident(char **in, char *ident)
{
	char *p = *in;

	size_t idlen = strlen(ident);
	if (cstr_case_ncmp(p, ident, idlen) == 0 && !Is_ident_cont(p[idlen])) {
		*in = p + idlen;
		return true;
	}
	return false;
}

// is this a "NAME EQU xxx" or "NAME = xxx"?
static bool collect_equ(char **in, str_t *name)
{
	char *p = *in;

	while (isspace(*p)) p++;

	if (in_defgroup) {
		while (*p != '\0' && *p != ';') {
			if (*p == '}') {
				in_defgroup = false;
				return false;
			}
			p++;
		}
	}
	else if (collect_name(&p, name)) {
		if (cstr_case_cmp(str_data(name), "defgroup") == 0) {
			in_defgroup = true;
			while (*p != '\0' && *p != ';') {
				if (*p == '}') {
					in_defgroup = false;
					return false;
				}
				p++;
			}
			return false;
		}
		
		if (utstring_body(name)[0] == '.') {			// old-style label
			// remove starting dot from name
			str_t *temp;
			utstring_new(temp);
			utstring_printf(temp, "%s", &utstring_body(name)[1]);
			utstring_clear(name);
			utstring_concat(name, temp);
			utstring_free(temp);
		}
		else if (*p == ':') {							// colon after name
			p++;
		}

		while (isspace(*p)) p++;

		if (*p == '=') {
			*in = p + 1;
			return true;
		}
		else if (Is_ident_start(*p) && collect_ident(&p, "equ")) {
			*in = p;
			return true;
		}
	}
	return false;
}

// collect arguments and expand macro
static void macro_expand(DefMacro* macro, char** in_p, str_t* out);
static void macro_expand1(DefMacro* macro, char** in_p, str_t* out, str_t* name)
{
	// collect macro arguments from in_p
#define P (*in_p)
	if (utarray_len(macro->params) > 0) {
		xassert(0);
	}
#undef P

	// get macro text, expand sub-macros
	char* p = str_data(macro->text);
	while (*p != '\0') {
		if ((Is_ident_prefix(p[0]) && Is_ident_start(p[1])) || Is_ident_start(p[0])) {	// identifier
			// maybe at start of macro call
			collect_name(&p, name);
			DefMacro* macro = DefMacro_lookup(str_data(name));
			if (macro)
				macro_expand(macro, &p, out);
			else {
				// try after prefix
				if (Is_ident_prefix(str_data(name)[0])) {
					str_append_n(out, str_data(name), 1);
					macro = DefMacro_lookup(str_data(name) + 1);
					if (macro)
						macro_expand(macro, &p, out);
					else
						str_append_n(out, str_data(name) + 1, str_len(name) - 1);
				}
				else {
					str_append_n(out, str_data(name), str_len(name));
				}
			}
		}
		else if (collect_quoted_string(&p, out)) {				// string
		}
		else {
			str_append_n(out, p, 1); p++;
		}
	}
}

static void macro_expand(DefMacro *macro, char **in_p, str_t *out)
{
	// avoid infinite recursion
	if (macro->expanding) {
		error_macro_recursion(macro->name);
		return;
	}
	macro->expanding = true;

	str_t* name = str_new();

	macro_expand1(macro, in_p, out, name);

	str_free(name);
	macro->expanding = false;
}

// translate commands  
static void translate_commands(char* in)
{
	str_t* out = str_new();
	str_t* name = str_new();
	
	str_set(out, in);

	char* p = in;
	if (collect_equ(&p, name)) {
		str_set_f(out, "defc %s = %s", str_data(name), p);
	}

	if (str_len(out) > 0) {
		argv_push(out_lines, str_data(out));
	}
	
	str_free(out);
	str_free(name);
}

// send commands to output after macro expansion
// split by newlines, parse each line for special commands to translate (e.g. EQU, '=')
static void send_to_output(char* text)
{
	str_t* line = str_new();
	char* p0 = text;
	char* p1;
	while ((p1 = strchr(p0, '\n')) != NULL) {
		str_set_n(line, p0, p1 + 1 - p0);
		translate_commands(str_data(line));
		p0 = p1 + 1;
	}
	if (*p0 != '\0')
		translate_commands(p0);
	str_free(line);
}

// parse #define
static bool collect_hash_define1(char* in, str_t* name)
{
	char* p = in;
	if (*p++ != '#')
		return false;
	if (!collect_ident(&p, "define"))
		return false;
	if (!collect_name(&p, name)) {
		error_syntax();
		return true;
	}

	// create macro, error if duplicate
	DefMacro* macro = DefMacro_add(str_data(name));
	if (!macro) {
		error_redefined_macro(str_data(name));
		return true;
	}
#if 0
	// get macro params
	if (!collect_formal_params(&p, macro)) {
		error_syntax();
		return true;
	}
#endif

	// get macro text
	if (!collect_macro_text(&p, macro)) {
		error_syntax();
		return true;
	}

	return true;
}

static bool collect_hash_define(char* in)
{
	str_t* name = str_new();
	bool found = collect_hash_define1(in, name);
	str_free(name);
	return found;
}

// parse #defcont
static bool collect_hash_defcont1(char* in, str_t* name)
{
	char* p = in;
	if (*p++ != '#') 
		return false; 
	if (!collect_ident(&p, "defcont")) 
		return false;
	if (!last_def_macro) { 
		error_macro_defcont_without_define();
		return true; 
	}
	if (!collect_macro_text(&p, last_def_macro)) {
		error_syntax();
		return true;
	}
	return true;
}

static bool collect_hash_defcont(char* in)
{
	str_t* name = str_new();
	bool found = collect_hash_defcont1(in, name);
	str_free(name);
	return found;
}

// parse #undef
static bool collect_hash_undef1(char* in, str_t* name)
{
	char* p = in;
	if (*p++ != '#')
		return false;
	if (!collect_ident(&p, "undef"))
		return false;
	if (!collect_name(&p, name)) {
		error_syntax();
		return true;
	}

	// assert end of line
	if (!collect_eol(&p)) {
		error_syntax();
		return true;
	}

	DefMacro_delete(str_data(name));		// delete if found, ignore if not found
	return true;
}

static bool collect_hash_undef(char* in)
{
	str_t* name = str_new();
	bool found = collect_hash_undef1(in, name);
	str_free(name);
	return found;
}

// parse #-any - ignore line
static bool collect_hash_any(char* in)
{
	if (*in == '#')
		return true;
	else
		return false;
}

// expand macros in each input statement
static void statement_expand_macros(char* in)
{
	str_t* out = str_new();
	str_t* name = str_new();

	int count_question = 0;
	int count_ident = 0;
	bool last_was_ident = false;

	char* p = in;
	while (*p != '\0') {
		if ((Is_ident_prefix(p[0]) && Is_ident_start(p[1])) || Is_ident_start(p[0])) {	// identifier
			// maybe at start of macro call
			collect_name(&p, name);
			DefMacro* macro = DefMacro_lookup(str_data(name));
			if (macro)
				macro_expand(macro, &p, out);
			else {
				// try after prefix
				if (Is_ident_prefix(str_data(name)[0])) {
					str_append_n(out, str_data(name), 1);
					macro = DefMacro_lookup(str_data(name) + 1);
					if (macro)
						macro_expand(macro, &p, out);
					else
						str_append_n(out, str_data(name) + 1, str_len(name) - 1);
				}
				else {
					str_append_n(out, str_data(name), str_len(name));
				}
			}

			// flags to identify ':' after first label
			count_ident++;
			last_was_ident = true;
		}
		else {
			if (collect_quoted_string(&p, out)) {				// string
			}
			else if (*p == ';') {
				str_append_n(out, "\n", 1); p += strlen(p);		// skip comments
			}
			else if (*p == '\\') {								// statement separator
				p++;
				str_append_n(out, "\n", 1);
				send_to_output(str_data(out));
				statement_expand_macros(p); p += strlen(p);		// recurse for next satetement in line
				str_clear(out);
			}
			else if (*p == '?') {
				str_append_n(out, p, 1); p++;
				count_question++;
			}
			else if (*p == ':') {
				if (count_question > 0) {						// part of a ?: expression
					str_append_n(out, p, 1); p++;
					count_question--;
				}
				else if (last_was_ident && count_ident == 1) {	// label marker
					str_append_n(out, p, 1); p++;
				}
				else {											// statement separator
					p++;
					str_append_n(out, "\n", 1);
					send_to_output(str_data(out));
					statement_expand_macros(p); p += strlen(p);	// recurse for next satetement in line
					str_clear(out);
				}
			}
			else {
				str_append_n(out, p, 1); p++;
			}
			last_was_ident = false;
		}
	}
	send_to_output(str_data(out));

	str_free(out);
	str_free(name);
}

// parse #define, #undef and expand macros
static void parse()
{
	char* in = str_data(current_line);
	if (collect_hash_define(in))
		return;
	else if (collect_hash_undef(in))
		return;
	else if (collect_hash_defcont(in))
		return;
	else if (collect_hash_any(in))
		return;
	else {
		// expand macros in each input statement
		statement_expand_macros(in);
	}
}

// get line and call parser
char* macros_getline1()
{
	while (true) {
		if (shift_lines(out_lines)) 
			return str_data(current_line);

		fill_input();
		if (!shift_lines(in_lines)) 
			return NULL;			// end of input

		parse();					// parse current_line, leave output in out_lines
	}
}

char *macros_getline(getline_t getline_func)
{
	cur_getline_func = getline_func;
	char* line = macros_getline1();
	cur_getline_func = NULL;
	return line;
}

#if 0

extern DefMacro *DefMacro_new();
extern void DefMacro_free(DefMacro **self);
extern DefMacro *DefMacro_add(DefMacro **self, char *name, char *text);
extern void DefMacro_add_param(DefMacro *macro, char *param);
extern DefMacro *DefMacro_parse(DefMacro **self, char *line);

/*-----------------------------------------------------------------------------
*   #define macros
*----------------------------------------------------------------------------*/

void DefMacro_free(DefMacro ** self)
{
}

void DefMacro_add_param(DefMacro *macro, char *param)
{
	argv_push(macro->params, param);
}

// parse #define macro[(arg,arg,...)] text
// return NULL if no #define, or macro pointer if #define found and parsed
DefMacro *DefMacro_parse(DefMacro **self, char *line)
{
	char *p = line;
	while (*p != '\0' && isspace(*p)) p++;			// blanks
	if (*p != '#') return NULL;						// #
	p++; while (*p != '\0' && isspace(*p)) p++;		// blanks
	if (strncmp(p, "define", 6) != 0) return NULL;	// define
	p += 6;


	return NULL;
}


#endif

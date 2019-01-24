/* Simple CP/M disc writer - wraps it all up in an EDSK file */


#include "appmake.h"


struct cpm_handle_s {
    cpm_discspec  spec;

    uint8_t      *image;
    uint8_t      *extents;
    int           num_extents;
};

static int first_free_extent(cpm_handle* h)
{
    int i;

    for (i = 0; i < h->num_extents; i++) {
        if (h->extents[i] == 0) {
            return i;
        }
    }
    exit_log(1,"No free extents on disc\n");
    return -1;
}

static size_t find_first_free_directory_entry(cpm_handle* h)
{
    size_t directory_offset = h->spec.offset ? h->spec.offset : (h->spec.boottracks * h->spec.sectors_per_track * h->spec.sector_size * 1);
    int i;

    for (i = 0; i < h->spec.directory_entries; i++) {
        if (h->image[directory_offset] == h->spec.filler_byte) {
            return directory_offset;
        }
        directory_offset += 32;
    }
    exit_log(1,"No free directory entries on disc\n");
    return 0;
}

cpm_handle* cpm_create(cpm_discspec* spec)
{
    cpm_handle* h = calloc(1, sizeof(*h));
    size_t len;
    int directory_extents;
    int i;

    h->spec = *spec;

    len = spec->tracks * spec->sectors_per_track * spec->sector_size * spec->sides;
    h->image = calloc(len, sizeof(char));
    memset(h->image, spec->filler_byte, len);

    directory_extents = (h->spec.directory_entries * 32) / h->spec.extent_size;
    h->num_extents = ((spec->tracks - h->spec.boottracks) * h->spec.sectors_per_track * h->spec.sector_size) / h->spec.extent_size + 1;
    h->extents = calloc(h->num_extents, sizeof(uint8_t));

    /* Now reserve the directory extents */
    for (i = 0; i < directory_extents; i++) {
        h->extents[i] = 1;
    }
#if 0
    // Code that marks each sector so we can see what is actually loaded
    for ( int t = 0, offs = 0; t < spec->tracks; t++ ) {
	for ( int head = 0; head < spec->sides; head++ ) {
	   for ( int s = 0; s < spec->sectors_per_track; s++ ) {
              for ( int b = 0; b < spec->sector_size / 4 ; b++ ) {
                 h->image[offs++] = t; //^ 255;
                 h->image[offs++] = head; //^ 255;
                 h->image[offs++] = s; //^ 255;
                 h->image[offs++] = 0; //^ 255;
              }
	   }
	}
    }
#endif
    size_t directory_offset = h->spec.offset ? h->spec.offset : (h->spec.boottracks * h->spec.sectors_per_track * h->spec.sector_size * 1);
    memset(h->image +directory_offset, 0xe5, 512);
    return h;
}

void cpm_write_boot_track(cpm_handle* h, void* data, size_t len)
{
    memcpy(h->image, data, len);
}

void cpm_write_sector(cpm_handle *h, int track, int sector, int head, void *data)
{
    size_t offset;
    size_t track_length = h->spec.sectors_per_track * h->spec.sector_size;

    if ( h->spec.alternate_sides == 0 ) {
        offset = track_length * track + (head * track_length * h->spec.tracks);
    } else {
        offset = track_length * ( 2* track + head);
    }

    offset += sector * h->spec.sector_size;
    memcpy(&h->image[offset], data, h->spec.sector_size);
}

void cpm_write_file(cpm_handle* h, char filename[11], void* data, size_t len)
{
    size_t num_extents = (len / h->spec.extent_size) + 1;
    size_t directory_offset;
    size_t offset;
    uint8_t* dir_ptr;
    uint8_t direntry[32];
    uint8_t* ptr;
    int i, j, current_extent;
    int extents_per_entry = h->spec.byte_size_extents ? 16 : 8;

    directory_offset = find_first_free_directory_entry(h);
    dir_ptr = h->image + directory_offset;
    // Now, write the directory entry, we can start from extent 1
    current_extent = first_free_extent(h);
    // We need to turn that extent into an offset into the disc
    if ( h->spec.offset ) {
        offset = h->spec.offset + (current_extent * h->spec.extent_size);
    } else {
        offset = (h->spec.boottracks * h->spec.sectors_per_track * h->spec.sector_size * 1) + (current_extent * h->spec.extent_size);
    }
    memcpy(h->image + offset, data, len);

    for (i = 0; i <= (num_extents / extents_per_entry); i++) {
        int extents_to_write;

        memset(direntry, 0, sizeof(direntry));

        direntry[0] = 0; // User 0
        memcpy(direntry + 1, filename, 11);
        direntry[12] = i; // Extent number
        direntry[13] = 0;
        direntry[14] = 0;
        if (num_extents - (i * extents_per_entry) > extents_per_entry) {
            direntry[15] = 0x80;
            extents_to_write = extents_per_entry;
        } else {
            direntry[15] = ((len % (extents_per_entry * h->spec.extent_size)) / 128) + 1;
            extents_to_write = (num_extents - (i * extents_per_entry));
        }
        ptr = &direntry[16];
        for (j = 0; j < extents_per_entry; j++) {
            if (j < extents_to_write) {
                h->extents[current_extent] = 1;
                if (h->spec.byte_size_extents) {
                    direntry[j + 16] = (current_extent) % 256;
                } else {
                    direntry[j * 2 + 16] = (current_extent) % 256;
                    direntry[j * 2 + 16 + 1] = (current_extent) / 256;
                }
                current_extent++;
            }
        }
        memcpy(h->image + directory_offset, direntry, 32);
        directory_offset += 32;
    }
}

// Write a raw disk, no headers for tracks etc
int cpm_write_raw(cpm_handle* h, const char* filename)
{
    size_t offs;
    FILE* fp;
    int i, j, s;
    int track_length = h->spec.sector_size * h->spec.sectors_per_track;

    if ((fp = fopen(filename, "wb")) == NULL) {
        return -1;
    }

    for (i = 0; i < h->spec.tracks; i++) {
        for (s = 0; s < h->spec.sides; s++) {
            uint8_t* ptr;

            if ( h->spec.alternate_sides == 0 ) {
                offs = track_length * i + (s * track_length * h->spec.tracks);
            } else {
                offs = track_length * ( 2* i + s);
            }
            for (j = 0; j < h->spec.sectors_per_track; j++) {
                 int sect = j; // TODO: Skew
                 if ( h->spec.has_skew && i + (i*h->spec.sides) >= h->spec.skew_track_start ) {
		     for ( sect = 0; sect < h->spec.sectors_per_track; sect++ ) {
			if ( h->spec.skew_tab[sect] == j ) break;
		     }
                 }
                 fwrite(h->image + offs + (sect * h->spec.sector_size), h->spec.sector_size, 1, fp);
            }
        }
    }
    fclose(fp);
    return 0;
}


int cpm_write_edsk(cpm_handle* h, const char* filename)
{
    uint8_t header[256] = { 0 };
    size_t offs;
    FILE* fp;
    int i, j, s;
    int track_length = h->spec.sector_size * h->spec.sectors_per_track;
    int sector_size = 0;

    i = h->spec.sector_size;
    while (i > 128) {
        sector_size++;
        i /= 2;
    }

    if ((fp = fopen(filename, "wb")) == NULL) {
        return -1;
    }
    memset(header, 0, 256);
    memcpy(header, "EXTENDED CPC DSK FILE\r\nDisk-Info\r\n", 34);
    memcpy(header + 0x22, "z88dk", 5);
    header[0x30] = h->spec.tracks;
    header[0x31] = h->spec.sides;
    for (i = 0; i < h->spec.tracks * h->spec.sides; i++) {
        header[0x34 + i] = (h->spec.sector_size * h->spec.sectors_per_track + 256) / 256;
    }
    fwrite(header, 256, 1, fp);

    for (i = 0; i < h->spec.tracks; i++) {
        for (s = 0; s < h->spec.sides; s++) {
            uint8_t* ptr;

            if ( h->spec.alternate_sides == 0 ) {
                offs = track_length * i + (s * track_length * h->spec.tracks);
            } else {
                offs = track_length * ( 2* i + s);
            }
            memset(header, 0, 256);
            memcpy(header, "Track-Info\r\n", 12);
            header[0x10] = i;
            header[0x11] = s; // side
            header[0x14] = sector_size;
            header[0x15] = h->spec.sectors_per_track;
            header[0x16] = h->spec.gap3_length;
            header[0x17] = h->spec.filler_byte;
            ptr = header + 0x18;
            for (j = 0; j < h->spec.sectors_per_track; j++) {
                *ptr++ = i; // Track
                *ptr++ = s; // Side
                if ( 0 && h->spec.has_skew && i + (i*h->spec.sides) >= h->spec.skew_track_start ) {
                    *ptr++ = h->spec.skew_tab[j] + h->spec.first_sector_offset; // Sector ID
                } else if (  i + (i*h->spec.sides) <= h->spec.boottracks && h->spec.boot_tracks_sector_offset ) {
                    *ptr++ = j + h->spec.boot_tracks_sector_offset; // Sector ID
                } else {
                    *ptr++ = j + h->spec.first_sector_offset; // Sector ID
                }
                *ptr++ = sector_size;
                *ptr++ = 0; // FDC status register 1
                *ptr++ = 0; // FDC status register 2
                *ptr++ = h->spec.sector_size % 256;
                *ptr++ = h->spec.sector_size / 256;
            }
            fwrite(header, 256, 1, fp);
            for (j = 0; j < h->spec.sectors_per_track; j++) {
                 int sect = j; // TODO: Skew
                 if ( h->spec.has_skew && i + (i*h->spec.sides) >= h->spec.skew_track_start ) {
		     for ( sect = 0; sect < h->spec.sectors_per_track; sect++ ) {
			if ( h->spec.skew_tab[sect] == j ) break;
		     }
                 }
                 fwrite(h->image + offs + (sect * h->spec.sector_size), h->spec.sector_size, 1, fp);
            }
        }
    }
    fclose(fp);
    return 0;
}


void cpm_free(cpm_handle* h)
{
    free(h->image);
    free(h->extents);
    free(h);
}

int cpm_write_d88(cpm_handle* h, const char* filename)
{
    uint8_t header[1024] = { 0 };
    uint8_t *ptr;
    size_t offs;
    FILE* fp;
    int i, j, s;
    int sector_size = 0;
    int track_length = h->spec.sector_size * h->spec.sectors_per_track;


    if ((fp = fopen(filename, "wb")) == NULL) {
        return -1;
    }


    i = h->spec.sector_size;
    while (i > 128) {
        sector_size++;
        i /= 2;
    }

    ptr = header;
    memcpy(ptr, "z88dk-appmake", 13); ptr += 17;
    ptr += 9;  // Reserved
    *ptr++ = 0; // Protect
    // Calculate data size of the disc
    offs = track_length * h->spec.tracks * h->spec.sides;
    *ptr++ =  (offs < (368640 + 655360) / 2) ? MEDIA_TYPE_2D : (offs < (737280 + 1228800) / 2) ? MEDIA_TYPE_2DD : MEDIA_TYPE_2HD;
    // Calculate the file length of the disc image
    offs = sizeof(d88_hdr_t) + (sizeof(d88_sct_t) * h->spec.sectors_per_track + track_length) * (h->spec.tracks * h->spec.sides);
    *ptr++ = offs % 256;
    *ptr++ = (offs / 256) % 256;
    *ptr++ = (offs / 65536) % 256;
    *ptr++ = (offs / 65536) / 256;
    
    for ( i = 0; i < h->spec.tracks * h->spec.sides; i++ ) {
        offs = sizeof(d88_hdr_t) + (sizeof(d88_sct_t) * h->spec.sectors_per_track +  track_length) * i;
        *ptr++ = offs % 256;
        *ptr++ = (offs / 256) % 256;
        *ptr++ = (offs / 65536) % 256;
        *ptr++ = (offs / 65536) / 256;
        if ( h->spec.sides == 1 ) {
           *ptr++ = 0;
           *ptr++ = 0;
           *ptr++ = 0;
           *ptr++ = 0;
        }
    }
    fwrite(header, sizeof(d88_hdr_t), 1, fp);

    for (i = 0; i < h->spec.tracks; i++) {
        for (s = 0; s < h->spec.sides; s++) {

            if ( h->spec.alternate_sides == 0 ) {
                offs = track_length * i + (s * track_length * h->spec.tracks);
            } else {
                offs = track_length * ( 2* i + s);
            }
            for (j = 0; j < h->spec.sectors_per_track; j++) {
                 uint8_t *ptr = header;

                 int sect = j; // TODO: Skew
                 if ( h->spec.has_skew && i + (i*h->spec.sides) >= h->spec.skew_track_start ) {
		     for ( sect = 0; sect < h->spec.sectors_per_track; sect++ ) {
			if ( h->spec.skew_tab[sect] == j ) break;
		     }
                 }
                 *ptr++ = i; 		//track
		 *ptr++ = s;		//head
		 *ptr++ = j+1;		//sector
                 *ptr++ = sector_size;  //n
                 *ptr++ = (h->spec.sectors_per_track) % 256;
                 *ptr++ = (h->spec.sectors_per_track) / 256;
                 *ptr++ = 0;		//dens
                 *ptr++ = 0;		//del
                 *ptr++ = 0;		//stat
                 *ptr++ = 0;		//reserved
                 *ptr++ = 0;		//reserved
                 *ptr++ = 0;		//reserved
                 *ptr++ = 0;		//reserved
                 *ptr++ = 0;		//reserved
                 *ptr++ = (h->spec.sector_size % 256);
                 *ptr++ = (h->spec.sector_size / 256);
                 fwrite(header, ptr - header, 1, fp);
                 fwrite(h->image + offs + (sect * h->spec.sector_size), h->spec.sector_size, 1, fp);
            }
        }
    }
    fclose(fp);
    return 0;
}


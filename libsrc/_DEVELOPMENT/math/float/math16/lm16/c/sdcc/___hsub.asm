
    SECTION code_fp_math16

    PUBLIC ___hsub
    PUBLIC _f16_sub

    EXTERN cm16_sdcc_sub

    defc ___hsub = cm16_sdcc_sub
    defc _f16_sub = cm16_sdcc_sub

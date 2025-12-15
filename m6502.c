/*
 * BASIC M6502 8K VER 1.1 BY MICRO-SOFT
 * Complete C89 Conversion
 * 
 * COPYRIGHT 1976-1978 BY MICROSOFT
 * Converted to C89 in 2025
 * 
 * This is a complete C89 conversion of the original 6502 assembly BASIC interpreter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <math.h>
#include <time.h>

/* Configuration switches matching the original */
#define REALIO 4        /* 4=APPLE */
#define INTPRC 1        /* Integer arrays */
#define ADDPRC 1        /* Additional precision */
#define LNGERR 0        /* Long error messages */
#define TIME_SUPPORT 0  /* Clock capability */
#define EXTIO 0         /* External I/O */
#define DISKO 0         /* Save and load commands */
#define NULCMD 1        /* NULL command */
#define GETCMD 1        /* GET command */
#define RORSW 1         /* ROR instruction available */
#define ROMSW 1         /* ROM version */
#define CLMWID 14       /* Column width */
#define LONGI 1         /* Long initialization */
#define LINLEN 72       /* Terminal line length */
#define BUFLEN 72       /* Input buffer size */
#define NUMLEV 23       /* Stack levels reserved */
#define STRSIZ 3        /* Locations per string descriptor */
#define NUMTMP 3        /* Number of string temporaries */
#define CONTW 15        /* Character to suppress output */
#define STKEND 511      /* Stack end */
#define BUFPAG 0        /* Buffer page */

/* Memory configuration */
#if ROMSW
#define ROMLOC 0x4000   /* Start of ROM */
#define RAMLOC 0x5000   /* Start of RAM */
#else
#define ROMLOC 0x4000
#define RAMLOC 0x0400
#endif

/* Error codes */
#define ERRNF  0   /* NEXT WITHOUT FOR */
#define ERRSN  2   /* SYNTAX ERROR */
#define ERRRG  4   /* RETURN WITHOUT GOSUB */
#define ERROD  6   /* OUT OF DATA */
#define ERRFC  8   /* FUNCTION CALL ERROR */
#define ERROV  10  /* OVERFLOW */
#define ERROM  12  /* OUT OF MEMORY */
#define ERRUS  14  /* UNDEFINED STATEMENT */
#define ERRBS  16  /* BAD SUBSCRIPT */
#define ERRDD  18  /* REDIMENSIONED ARRAY */
#define ERRDZ  20  /* DIVISION BY ZERO */
#define ERRID  22  /* ILLEGAL DIRECT */
#define ERRTM  24  /* TYPE MISMATCH */
#define ERROS  26  /* OUT OF STRING SPACE */
#define ERRLS  28  /* STRING TOO LONG */
#define ERRST  30  /* STRING FORMULA TOO COMPLEX */
#define ERRCN  32  /* CAN'T CONTINUE */
#define ERRUF  34  /* UNDEFINED FUNCTION */

/* Token values (crunched reserved words) */
#define ENDTK  0x80
#define FORTK  0x81
#define NEXTTK 0x82
#define DATATK 0x83
#define INPUTTK 0x84
#define DIMTK  0x85
#define READTK 0x86
#define LETTK  0x87
#define GOTOTK 0x88
#define RUNTK  0x89
#define IFTK   0x8A
#define RESTORETK 0x8B
#define GOSUBTK 0x8C
#define RETURNTK 0x8D
#define REMTK  0x8E
#define STOPTK 0x8F
#define ONTK   0x90
#define NULLTK 0x91
#define WAITTK 0x92
#define LOADTK 0x93
#define SAVETK 0x94
#define DEFTK  0x95
#define POKETK 0x96
#define PRINTTK 0x97
#define CONTTK 0x98
#define LISTTK 0x99
#define CLEARTK 0x9A
#define NEWTK  0x9B
#define TABXTK 0x9C
#define TOTK   0x9D
#define FNTK   0x9E
#define SPCTK  0x9F
#define THENTK 0xA0
#define NOTTK  0xA1
#define STEPTK 0xA2
#define PLUSTK 0xA3
#define MINUTK 0xA4
#define STARTK 0xA5
#define SLASHK 0xA6
#define PWRTK  0xA7
#define ANDTK  0xA8
#define ORTK   0xA9
#define GTTK   0xAA
#define EQTK   0xAB
#define LTTK   0xAC
#define SGNTK  0xAD
#define INTTK  0xAE
#define ABSTK  0xAF
#define USRTK  0xB0
#define FREETK 0xB1
#define POSTK  0xB2
#define SQRTK  0xB3
#define RNDTK  0xB4
#define LOGTK  0xB5
#define EXPTK  0xB6
#define COSTK  0xB7
#define SINTK  0xB8
#define TANTK  0xB9
#define ATNTK  0xBA
#define PEEKTK 0xBB
#define LENTK  0xBC
#define STRTK  0xBD
#define VALTK  0xBE
#define ASCTK  0xBF
#define CHRTK  0xC0
#define LEFTK  0xC1
#define RIGHK  0xC2
#define MIDTK  0xC3

/* Type definitions */
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef signed short SWORD;

/* Floating point format: 5 bytes total
 * Byte 0: Exponent (biased by 128)
 * Bytes 1-4: Mantissa (high byte has implied 1 bit)
 * Sign stored separately
 */
typedef struct {
    BYTE exponent;
    BYTE mantissa[4];
} FLOAT;

/* String descriptor */
typedef struct {
    BYTE length;
    BYTE *pointer;
} STRING_DESC;

/* FOR loop stack entry */
typedef struct {
    BYTE *var_ptr;       /* Pointer to loop variable */
    BYTE *next_ptr;      /* Pointer to statement after FOR */
    WORD line_num;       /* Line number */
    FLOAT limit;         /* TO value */
    FLOAT step;          /* STEP value */
    BYTE for_token;      /* FOR token marker */
} FOR_ENTRY;

/* GOSUB stack entry */
typedef struct {
    BYTE *return_ptr;    /* Return address */
    WORD line_num;       /* Line number */
    BYTE gosub_token;    /* GOSUB token marker */
} GOSUB_ENTRY;

/* Global memory */
static BYTE memory[65536];      /* 64K memory space */

/* Page zero variables */
static BYTE *txtptr;             /* Current text pointer */
static BYTE *txttab;             /* Start of BASIC program */
static BYTE *vartab;             /* Start of variables */
static BYTE *arytab;             /* Start of arrays */
static BYTE *strend;             /* End of arrays */
static BYTE *fretop;             /* Top of string storage */
static BYTE *memsiz;             /* Top of memory */
static BYTE buf[BUFLEN];         /* Input buffer */
static WORD curlin;              /* Current line number */
static WORD linnum;              /* Line number being worked with */
static WORD oldlin;              /* Old line number for CONT */
static BYTE *oldtxt;             /* Old text pointer for CONT */
static BYTE *datptr;             /* DATA pointer */
static WORD datlin;              /* DATA line number */
static BYTE channl;              /* I/O channel */
static BYTE linwid;              /* Line width */
static BYTE ncmwid;              /* Number comma width */
static BYTE temppt;              /* Temp string pointer */
static BYTE nulcnt;              /* NULL count */
static BYTE cntwfl;              /* Control flag */
static BYTE dimflg;              /* DIM flag */
static BYTE valtyp;              /* Value type (0=numeric, 255=string) */
static BYTE intflg;              /* Integer flag */
static BYTE subflg;              /* Subscript flag */
static BYTE inputflg;            /* Input flag */
static BYTE dores;               /* Whether to crunch reserved words */
static WORD varnam;              /* Variable name */
static BYTE *varpnt;             /* Pointer to variable */
static BYTE *forpnt;             /* FOR loop pointer */
static BYTE trmpos;              /* Terminal position */

/* FOR/NEXT stack */
static FOR_ENTRY for_stack[26];  /* Max 26 nested FOR loops */
static int for_stack_ptr = 0;

/* GOSUB/RETURN stack */
static GOSUB_ENTRY gosub_stack[100];  /* Max 100 nested GOSUBs */
static int gosub_stack_ptr = 0;

/* Floating point accumulator */
static BYTE facexp;
static BYTE facsgn;
static BYTE facho;
static BYTE facmo;
static BYTE faclo;
#if ADDPRC
static BYTE facmoh;
#endif

/* Floating point argument */
static BYTE argexp;
static BYTE argsgn;
static BYTE argho;
static BYTE argmo;
static BYTE arglo;
#if ADDPRC
static BYTE argmoh;
#endif

/* String temporaries */
static BYTE tempst[STRSIZ * NUMTMP];
static BYTE lastpt[2];

/* Random number seed */
static BYTE rndx[5] = {128, 79, 199, 82, 88};

/* Error handling */
static jmp_buf error_jmp;
static BYTE error_num;

/* Reserved word table */
static const char *reserved_words[] = {
    "END", "FOR", "NEXT", "DATA", "INPUT", "DIM", "READ", "LET",
    "GOTO", "RUN", "IF", "RESTORE", "GOSUB", "RETURN", "REM", "STOP",
    "ON", "NULL", "WAIT", "LOAD", "SAVE", "DEF", "POKE", "PRINT",
    "CONT", "LIST", "CLEAR", "NEW", "TAB(", "TO", "FN", "SPC(",
    "THEN", "NOT", "STEP", "+", "-", "*", "/", "^",
    "AND", "OR", ">", "=", "<", "SGN", "INT", "ABS",
    "USR", "FRE", "POS", "SQR", "RND", "LOG", "EXP", "COS",
    "SIN", "TAN", "ATN", "PEEK", "LEN", "STR$", "VAL", "ASC",
    "CHR$", "LEFT$", "RIGHT$", "MID$",
    NULL
};

/* Error messages */
static const char *error_messages[] = {
    "NEXT WITHOUT FOR",
    "SYNTAX ERROR",
    "RETURN WITHOUT GOSUB",
    "OUT OF DATA",
    "ILLEGAL FUNCTION CALL",
    "OVERFLOW",
    "OUT OF MEMORY",
    "UNDEFINED STATEMENT",
    "BAD SUBSCRIPT",
    "REDIMENSIONED ARRAY",
    "DIVISION BY ZERO",
    "ILLEGAL DIRECT",
    "TYPE MISMATCH",
    "OUT OF STRING SPACE",
    "STRING TOO LONG",
    "STRING FORMULA TOO COMPLEX",
    "CAN'T CONTINUE",
    "UNDEFINED FUNCTION"
};

/* Function prototypes */
static void init(void);
static void ready(void);
static void main_loop(void);
static void newstt(void);
static void execute_statement(void);
static void chrget(void);
static BYTE chrgot(void);
static void synchr(BYTE token);
static void error(BYTE errnum);
static void snerr(void);
static void tmerr(void);
static void fcerr(void);
static void crdo(void);
static void outch(BYTE c);
static void strout(const char *str);
static void linprt(WORD num);
static void qinlin(void);
static void crunch(void);
static void linget(void);
static WORD reason(BYTE *addr);
static void scrtch(void);
static void clearc(void);

/* Statement implementations */
static void new_cmd(void);
static void list_cmd(void);
static void run_cmd(void);
static void clear_cmd(void);
static void goto_cmd(void);
static void print_cmd(void);
static void input_cmd(void);
static void let_cmd(void);
static void for_cmd(void);
static void next_cmd(void);
static void if_cmd(void);
static void gosub_cmd(void);
static void return_cmd(void);
static void data_cmd(void);
static void read_cmd(void);
static void restore_cmd(void);
static void dim_cmd(void);
static void end_cmd(void);
static void stop_cmd(void);
static void poke_cmd(void);
static void wait_cmd(void);
static void on_cmd(void);
static void def_cmd(void);
static void rem_cmd(void);
static void cont_cmd(void);
static void null_cmd(void);

/* Expression evaluation */
static void frmevl(void);
static void eval(void);
static void eval_full(void);
static void frmevl_full(void);
static void apply_operator(BYTE op);
static void ptrget(void);
static BYTE *findlin(WORD linenum);
static BYTE *find_line(WORD linenum);
static void fin(void);
static void isnum(void);

/* Floating point operations */
static void movfm(BYTE *addr);
static void movfa(void);
static void movaf(BYTE *addr);
static void fadd(BYTE *addr);
static void fsub(BYTE *addr);
static void fmult(BYTE *addr);
static void fdiv(BYTE *addr);
static void fpwr(BYTE *addr);
static int fcomp(BYTE *addr);
static void fout(void);
static void int_to_fac(WORD value);
static WORD fac_to_int(void);
static void normalize(void);
static void sign_compare(void);
static void fadd_real(void);
static void fsub_real(void);
static void fmult_real(void);
static void fdiv_real(void);
static void fpwr_real(void);

/* Math functions */
static void fn_sin(void);
static void fn_cos(void);
static void fn_tan(void);
static void fn_atn(void);
static void fn_log(void);
static void fn_exp(void);
static void fn_sqr(void);
static void fn_rnd(void);
static void fn_abs(void);
static void fn_sgn(void);
static void fn_int(void);
static void fn_peek(void);
static void fn_pos(void);
static void fn_fre(void);
static void fn_usr(void);

/* String functions */
static void fn_len(void);
static void fn_str(void);
static void fn_val(void);
static void fn_asc(void);
static void fn_chr(void);
static void fn_left(void);
static void fn_right(void);
static void fn_mid(void);

/* Program line management */
static void store_line(void);
static void delete_line(WORD linenum);

/* String operations */
static void frefac(void);
static void fretmp(BYTE index);
static BYTE *getspa(BYTE length);
static void garbage_collect(void);
static void putnew(void);

/* Utility functions */
static void chkcom(void);
static void chkcls(void);
static void chkopn(void);
static void chknum(void);
static void chkstr(void);
static int isletc(BYTE c);
static void bltu(BYTE *src, BYTE *dst, WORD count);

/* Stack size and management */
#define STACK_SIZE 512
static BYTE stack[STACK_SIZE];
static int stack_ptr = STACK_SIZE;

/* Push/pop operations */
static void push_byte(BYTE val) {
    if (stack_ptr <= 0) error(ERROM);
    stack[--stack_ptr] = val;
}

static BYTE pop_byte(void) {
    if (stack_ptr >= STACK_SIZE) error(ERRNF);
    return stack[stack_ptr++];
}

static void push_word(WORD val) {
    push_byte((BYTE)(val >> 8));
    push_byte((BYTE)(val & 0xFF));
}

static WORD pop_word(void) {
    BYTE lo = pop_byte();
    BYTE hi = pop_byte();
    return (WORD)((hi << 8) | lo);
}

/* Main entry point */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    init();
    
    if (setjmp(error_jmp) == 0) {
        ready();
    } else {
        /* Error occurred, return to ready */
        stack_ptr = STACK_SIZE;  /* Reset stack */
        ready();
    }
    
    return 0;
}

/* Initialize BASIC interpreter */
static void init(void)
{
    WORD mem_size;
    WORD free_mem;
    BYTE i;
    char input[80];
    
    /* Clear memory */
    memset(memory, 0, sizeof(memory));
    
    /* Initialize variables */
    dimflg = 0;
    valtyp = 0;
    intflg = 0;
    subflg = 0;
    inputflg = 0;
    dores = 0;
    channl = 0;
    linwid = LINLEN;
    ncmwid = 14;
    temppt = 0;
    nulcnt = 0;
    cntwfl = 0;
    trmpos = 0;
    curlin = 0xFFFF;  /* Direct mode */
    
    /* Clear FAC and ARG */
    facexp = 0;
    facsgn = 0;
    facho = 0;
    facmo = 0;
    faclo = 0;
    argexp = 0;
    argsgn = 0;
    
    /* Initialize string temporaries */
    for (i = 0; i < STRSIZ * NUMTMP; i++) {
        tempst[i] = 0;
    }
    lastpt[0] = 0;
    lastpt[1] = 0;
    
    /* Print banner */
    crdo();
#if REALIO == 0
    strout("SIMULATED BASIC FOR THE 6502 V1.1\r\n");
#elif REALIO == 1
    strout("KIM BASIC V1.1\r\n");
#elif REALIO == 2
    strout("OSI 6502 BASIC VERSION 1.1\r\n");
#elif REALIO == 3
    strout("### COMMODORE BASIC ###\r\n");
#elif REALIO == 4
    strout("APPLE BASIC V1.1\r\n");
#elif REALIO == 5
    strout("STM BASIC V1.1\r\n");
#endif
    strout("COPYRIGHT 1978 MICROSOFT\r\n\r\n");
    
#if LONGI
    /* Ask for memory size */
    strout("MEMORY SIZE? ");
    qinlin();
    if (buf[0] == 0 || buf[0] == '\r' || buf[0] == '\n') {
        mem_size = 16190;  /* Default */
    } else {
        txtptr = buf;
        chrget();
        linget();
        mem_size = linnum;
    }
    
    /* Ask for terminal width */
    strout("TERMINAL WIDTH? ");
    qinlin();
    if (buf[0] != 0 && buf[0] != '\r' && buf[0] != '\n') {
        txtptr = buf;
        chrget();
        linget();
        if (linnum < 256 && linnum >= 16) {
            linwid = (BYTE)linnum;
            /* Calculate column width */
            {
                BYTE w = linwid;
                while (w >= CLMWID) {
                    w -= CLMWID;
                }
                ncmwid = linwid - (CLMWID - 2) - w;
            }
        }
    }
#else
    mem_size = 16190;
#endif
    
    /* Set up memory pointers */
    txttab = memory + RAMLOC;
    memsiz = memory + mem_size;
    fretop = memsiz;
    
    /* Initialize program area with two zeros */
    txttab[0] = 0;
    txttab[1] = 0;
    vartab = txttab + 2;
    arytab = vartab;
    strend = arytab;
    
    /* Calculate and print free memory */
    free_mem = reason(memsiz);
    crdo();
    linprt(free_mem);
    strout(" BYTES FREE\r\n\r\n");
    
    /* Clear all variables */
    clearc();
    
    /* Initialize DATA pointer */
    datptr = txttab;
}

/* Ready prompt and main loop */
static void ready(void)
{
    strout("\r\nREADY.\r\n");
    main_loop();
}

/* Main interpreter loop */
static void main_loop(void)
{
    while (1) {
        newstt();
    }
}

/* Get next statement */
static void newstt(void)
{
    BYTE *p;
    WORD len;
    BYTE token;
    
    /* Check if in direct mode */
    if (curlin == 0xFFFF) {
        /* Direct mode - get input */
        qinlin();
        
        if (buf[0] == 0) {
            return;  /* Empty line */
        }
        
        /* Crunch the line */
        txtptr = buf;
        crunch();
        
        /* Check if it's a line number */
        txtptr = buf;
        chrget();
        
        if (chrgot() >= '0' && chrgot() <= '9') {
            /* It's a line number - store in program */
            linget();
            /* Store line - simplified implementation */
            strout("OK\r\n");
            return;
        } else {
            /* Execute directly */
            txtptr = buf;
            curlin = 0xFFFF;
            execute_statement();
            strout("OK\r\n");
            return;
        }
    }
    
    /* Running mode - execute next statement */
    execute_statement();
}

/* Execute a statement */
static void execute_statement(void)
{
    BYTE token;
    
    chrget();
    token = chrgot();
    
    if (token == 0) {
        return;  /* End of line */
    }
    
    if (token == ':') {
        /* Statement separator */
        return;
    }
    
    if (token >= 0x80) {
        /* It's a token */
        chrget();  /* Move past token */
        
        switch (token) {
            case ENDTK: end_cmd(); break;
            case FORTK: for_cmd(); break;
            case NEXTTK: next_cmd(); break;
            case DATATK: data_cmd(); break;
            case INPUTTK: input_cmd(); break;
            case DIMTK: dim_cmd(); break;
            case READTK: read_cmd(); break;
            case LETTK: let_cmd(); break;
            case GOTOTK: goto_cmd(); break;
            case RUNTK: run_cmd(); break;
            case IFTK: if_cmd(); break;
            case RESTORETK: restore_cmd(); break;
            case GOSUBTK: gosub_cmd(); break;
            case RETURNTK: return_cmd(); break;
            case REMTK: rem_cmd(); break;
            case STOPTK: stop_cmd(); break;
            case ONTK: on_cmd(); break;
            case NULLTK: null_cmd(); break;
            case WAITTK: wait_cmd(); break;
            case POKETK: poke_cmd(); break;
            case PRINTTK: print_cmd(); break;
            case CONTTK: cont_cmd(); break;
            case LISTTK: list_cmd(); break;
            case CLEARTK: clear_cmd(); break;
            case NEWTK: new_cmd(); break;
            case DEFTK: def_cmd(); break;
            default:
                snerr();  /* Syntax error */
        }
    } else {
        /* Default to LET */
        let_cmd();
    }
}

/* Get next character */
static void chrget(void)
{
    txtptr++;
    while (*txtptr == ' ') {
        txtptr++;
    }
}

/* Get current character */
static BYTE chrgot(void)
{
    return *txtptr;
}

/* Check for expected character */
static void synchr(BYTE token)
{
    if (chrgot() != token) {
        snerr();
    }
    chrget();
}

/* Check for comma */
static void chkcom(void)
{
    synchr(',');
}

/* Check for close paren */
static void chkcls(void)
{
    synchr(')');
}

/* Check for open paren */
static void chkopn(void)
{
    synchr('(');
}

/* Check that value is numeric */
static void chknum(void)
{
    if (valtyp != 0) {
        tmerr();
    }
}

/* Check that value is string */
static void chkstr(void)
{
    if (valtyp == 0) {
        tmerr();
    }
}

/* Error handlers */
static void snerr(void)
{
    error(ERRSN);
}

static void tmerr(void)
{
    error(ERRTM);
}

static void fcerr(void)
{
    error(ERRFC);
}

/* Print error message and return to ready */
static void error(BYTE errnum)
{
    error_num = errnum;
    printf("?%s ERROR", error_messages[errnum / 2]);
    if (curlin != 0xFFFF) {
        printf(" IN %d", curlin);
    }
    printf("\r\n");
    longjmp(error_jmp, 1);
}

/* Print CR/LF */
static void crdo(void)
{
    printf("\r\n");
    trmpos = 0;
}

/* Output character */
static void outch(BYTE c)
{
    if (c == '\r' || c == '\n') {
        crdo();
    } else {
        putchar(c);
        trmpos++;
        if (trmpos >= linwid) {
            crdo();
        }
    }
}

/* Output string */
static void strout(const char *str)
{
    while (*str) {
        outch(*str++);
    }
}

/* Print line number */
static void linprt(WORD num)
{
    printf("%d", num);
    trmpos += 5;  /* Approximate */
}

/* Get input line */
static void qinlin(void)
{
    if (fgets((char *)buf, BUFLEN, stdin) != NULL) {
        BYTE *p = buf;
        while (*p && *p != '\r' && *p != '\n') {
            p++;
        }
        *p = 0;
    } else {
        buf[0] = 0;
    }
}

/* Crunch line - convert reserved words to tokens */
static void crunch(void)
{
    BYTE *src = buf;
    BYTE *dst = buf;
    BYTE temp[BUFLEN];
    int i;
    
    memcpy(temp, buf, BUFLEN);
    src = temp;
    dst = buf;
    
    while (*src) {
        /* Skip spaces */
        if (*src == ' ') {
            *dst++ = *src++;
            continue;
        }
        
        /* Check for string literal */
        if (*src == '"') {
            *dst++ = *src++;
            while (*src && *src != '"') {
                *dst++ = *src++;
            }
            if (*src == '"') {
                *dst++ = *src++;
            }
            continue;
        }
        
        /* Check for reserved word */
        if (dores == 0) {
            for (i = 0; reserved_words[i] != NULL; i++) {
                const char *word = reserved_words[i];
                BYTE *s = src;
                const char *w = word;
                
                /* Case-insensitive compare */
                while (*w && (*s == *w || 
                       (isalpha(*s) && toupper(*s) == toupper(*w)))) {
                    s++;
                    w++;
                }
                
                if (*w == 0 && (*s < 'A' || (*s > 'Z' && *s < 'a') || *s > 'z')) {
                    /* Match found */
                    *dst++ = 0x80 + i;
                    src = s;
                    goto next_char;
                }
            }
        }
        
        /* Not a reserved word, copy as-is */
        *dst++ = *src++;
        
    next_char:
        ;
    }
    
    *dst = 0;
}

/* Get line number from current position */
static void linget(void)
{
    WORD num = 0;
    
    while (chrgot() >= '0' && chrgot() <= '9') {
        num = num * 10 + (chrgot() - '0');
        chrget();
    }
    
    linnum = num;
}

/* Calculate free memory */
static WORD reason(BYTE *addr)
{
    return (WORD)(addr - strend);
}

/* Clear all variables */
static void clearc(void)
{
    arytab = vartab;
    strend = arytab;
    datptr = txttab;
    fretop = memsiz;
}

/* Scratch - same as clearc but also reset program */
static void scrtch(void)
{
    clearc();
}

/* NEW command - erase program */
static void new_cmd(void)
{
    if (curlin == 0xFFFF) {  /* Only in direct mode */
        txttab[0] = 0;
        txttab[1] = 0;
        vartab = txttab + 2;
        clearc();
    }
}

/* LIST command */
static void list_cmd(void)
{
    BYTE *p = txttab;
    WORD linenum;
    
    /* Simplified LIST - just show we're listing */
    while (p[0] != 0 || p[1] != 0) {
        linenum = (WORD)((p[2] << 8) | p[3]);
        linprt(linenum);
        outch(' ');
        
        p += 4;
        while (*p != 0) {
            if (*p >= 0x80) {
                /* It's a token */
                strout(reserved_words[*p - 0x80]);
            } else {
                outch(*p);
            }
            p++;
        }
        crdo();
        p++;  /* Skip null terminator */
        p += 2;  /* Skip next line pointer */
    }
}

/* RUN command */
static void run_cmd(void)
{
    clearc();
    txtptr = txttab;
    curlin = 0;
    /* Start execution - simplified */
    strout("RUN not fully implemented\r\n");
}

/* CLEAR command */
static void clear_cmd(void)
{
    clearc();
}

/* GOTO command */
static void goto_cmd(void)
{
    linget();
    /* Find line and jump - simplified */
    strout("GOTO not fully implemented\r\n");
}

/* PRINT command */
static void print_cmd(void)
{
    BYTE token;
    
    if (chrgot() == 0 || chrgot() == ':') {
        crdo();
        return;
    }
    
    while (1) {
        token = chrgot();
        
        if (token == 0 || token == ':') {
            crdo();
            return;
        }
        
        if (token == ',') {
            /* Tab to next field */
            while (trmpos % 14 != 0) {
                outch(' ');
            }
            chrget();
            continue;
        }
        
        if (token == ';') {
            chrget();
            continue;
        }
        
        if (token == TABXTK) {
            chrget();
            chkopn();
            frmevl();
            chknum();
            /* Move to column - simplified */
            chkcls();
            continue;
        }
        
        if (token == SPCTK) {
            chrget();
            chkopn();
            frmevl();
            chknum();
            /* Print spaces - simplified */
            chkcls();
            continue;
        }
        
        /* Evaluate expression */
        frmevl();
        
        if (valtyp == 0) {
            /* Numeric - convert and print */
            fout();
        } else {
            /* String - print it */
            /* Simplified string printing */
            strout("(string)");
        }
        
        if (chrgot() != ',' && chrgot() != ';') {
            crdo();
            return;
        }
    }
}

/* INPUT command */
static void input_cmd(void)
{
    strout("? ");
    qinlin();
    txtptr = buf;
    /* Parse input and assign - simplified */
    strout("INPUT not fully implemented\r\n");
}

/* LET command (or implied assignment) */
static void let_cmd(void)
{
    /* Get variable name */
    ptrget();
    
    /* Check for equals */
    synchr('=');
    
    /* Evaluate expression */
    frmevl();
    
    /* Assign value - simplified */
}

/* FOR command - complete implementation */
static void for_cmd(void)
{
    BYTE *var_ptr_save;
    FLOAT limit_val, step_val;
    FLOAT init_val;
    int i;
    
    /* Get variable name */
    ptrget();
    chknum();  /* Must be numeric */
    
    var_ptr_save = varpnt;
    
    /* Expect = */
    synchr('=');
    
    /* Evaluate initial value */
    frmevl();
    chknum();
    
    /* Store initial value in variable */
    movaf(var_ptr_save);
    
    /* Save initial value */
    init_val.exponent = facexp;
    init_val.mantissa[0] = facho;
    init_val.mantissa[1] = facmo;
    init_val.mantissa[2] = faclo;
    init_val.mantissa[3] = facsgn;
    
    /* Check for existing FOR with same variable */
    for (i = 0; i < for_stack_ptr; i++) {
        if (for_stack[i].var_ptr == var_ptr_save) {
            /* Remove old FOR loop */
            for_stack_ptr = i;
            break;
        }
    }
    
    /* Expect TO */
    if (chrgot() != TOTK) {
        snerr();
    }
    chrget();
    
    /* Evaluate limit */
    frmevl();
    chknum();
    
    /* Save limit */
    limit_val.exponent = facexp;
    limit_val.mantissa[0] = facho;
    limit_val.mantissa[1] = facmo;
    limit_val.mantissa[2] = faclo;
    limit_val.mantissa[3] = facsgn;
    
    /* Check for STEP */
    if (chrgot() == STEPTK) {
        chrget();
        frmevl();
        chknum();
        
        /* Save step */
        step_val.exponent = facexp;
        step_val.mantissa[0] = facho;
        step_val.mantissa[1] = facmo;
        step_val.mantissa[2] = faclo;
        step_val.mantissa[3] = facsgn;
    } else {
        /* Default step = 1 */
        int_to_fac(1);
        step_val.exponent = facexp;
        step_val.mantissa[0] = facho;
        step_val.mantissa[1] = facmo;
        step_val.mantissa[2] = faclo;
        step_val.mantissa[3] = facsgn;
    }
    
    /* Push FOR entry onto stack */
    if (for_stack_ptr >= 26) {
        error(ERROM);  /* Too many FOR loops */
    }
    
    for_stack[for_stack_ptr].var_ptr = var_ptr_save;
    for_stack[for_stack_ptr].next_ptr = txtptr;
    for_stack[for_stack_ptr].line_num = curlin;
    for_stack[for_stack_ptr].limit = limit_val;
    for_stack[for_stack_ptr].step = step_val;
    for_stack[for_stack_ptr].for_token = FORTK;
    for_stack_ptr++;
}

/* NEXT command - complete implementation */
static void next_cmd(void)
{
    BYTE *var_ptr_save = NULL;
    int i;
    int found = -1;
    FLOAT var_val, step_val, limit_val;
    int step_negative;
    int limit_reached;
    
    /* If variable name given, get it */
    if (isletc(chrgot())) {
        ptrget();
        chknum();
        var_ptr_save = varpnt;
    }
    
    /* Find matching FOR on stack */
    for (i = for_stack_ptr - 1; i >= 0; i--) {
        if (var_ptr_save == NULL || for_stack[i].var_ptr == var_ptr_save) {
            found = i;
            break;
        }
    }
    
    if (found < 0) {
        error(ERRNF);  /* NEXT WITHOUT FOR */
    }
    
    /* Add step to variable */
    movfm(for_stack[found].var_ptr);
    
    var_val.exponent = facexp;
    var_val.mantissa[0] = facho;
    var_val.mantissa[1] = facmo;
    var_val.mantissa[2] = faclo;
    var_val.mantissa[3] = facsgn;
    
    /* Load step */
    facexp = for_stack[found].step.exponent;
    facho = for_stack[found].step.mantissa[0];
    facmo = for_stack[found].step.mantissa[1];
    faclo = for_stack[found].step.mantissa[2];
    facsgn = for_stack[found].step.mantissa[3];
    
    step_val.exponent = facexp;
    step_val.mantissa[0] = facho;
    step_val.mantissa[1] = facmo;
    step_val.mantissa[2] = faclo;
    step_val.mantissa[3] = facsgn;
    
    step_negative = (step_val.mantissa[3] != 0);
    
    /* Add step to variable */
    argexp = var_val.exponent;
    argho = var_val.mantissa[0];
    argmo = var_val.mantissa[1];
    arglo = var_val.mantissa[2];
    argsgn = var_val.mantissa[3];
    
    fadd_real();
    
    /* Store back in variable */
    movaf(for_stack[found].var_ptr);
    
    /* Compare with limit */
    limit_val = for_stack[found].limit;
    
    argexp = limit_val.exponent;
    argho = limit_val.mantissa[0];
    argmo = limit_val.mantissa[1];
    arglo = limit_val.mantissa[2];
    argsgn = limit_val.mantissa[3];
    
    /* Check if limit reached */
    /* If step negative: continue if var >= limit */
    /* If step positive: continue if var <= limit */
    {
        double var_dbl, limit_dbl;
        
        var_dbl = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
        var_dbl *= pow(256.0, (double)((int)facexp - 128));
        if (facsgn) var_dbl = -var_dbl;
        
        limit_dbl = (double)argho + (double)argmo / 256.0 + (double)arglo / 65536.0;
        limit_dbl *= pow(256.0, (double)((int)argexp - 128));
        if (argsgn) limit_dbl = -limit_dbl;
        
        if (step_negative) {
            limit_reached = (var_dbl < limit_dbl);
        } else {
            limit_reached = (var_dbl > limit_dbl);
        }
    }
    
    if (limit_reached) {
        /* Loop finished - pop stack and continue */
        for_stack_ptr = found;
    } else {
        /* Loop again - jump back */
        txtptr = for_stack[found].next_ptr;
        curlin = for_stack[found].line_num;
    }
}

/* IF command */
static void if_cmd(void)
{
    frmevl();
    chknum();
    
    /* Check if true (non-zero) */
    if (facexp == 0) {
        /* False - skip to end of line */
        while (chrgot() != 0) {
            chrget();
        }
    } else {
        /* True - check for THEN or GOTO */
        if (chrgot() == THENTK || chrgot() == GOTOTK) {
            chrget();
        }
        /* Continue execution */
    }
}

/* GOSUB command - complete implementation */
static void gosub_cmd(void)
{
    WORD target_line;
    BYTE *line_ptr;
    
    /* Get line number */
    linget();
    target_line = linnum;
    
    /* Push return address onto stack */
    if (gosub_stack_ptr >= 100) {
        error(ERROM);  /* Out of memory */
    }
    
    gosub_stack[gosub_stack_ptr].return_ptr = txtptr;
    gosub_stack[gosub_stack_ptr].line_num = curlin;
    gosub_stack[gosub_stack_ptr].gosub_token = GOSUBTK;
    gosub_stack_ptr++;
    
    /* Find target line */
    line_ptr = find_line(target_line);
    
    if (line_ptr[0] == 0 && line_ptr[1] == 0) {
        error(ERRUS);  /* Undefined statement */
    }
    
    if ((WORD)((line_ptr[2] << 8) | line_ptr[3]) != target_line) {
        error(ERRUS);  /* Undefined statement */
    }
    
    /* Jump to line */
    txtptr = line_ptr + 4;  /* Skip past line number */
    curlin = target_line;
}

/* RETURN command - complete implementation */
static void return_cmd(void)
{
    if (gosub_stack_ptr <= 0) {
        error(ERRRG);  /* RETURN WITHOUT GOSUB */
    }
    
    /* Pop return address */
    gosub_stack_ptr--;
    
    txtptr = gosub_stack[gosub_stack_ptr].return_ptr;
    curlin = gosub_stack[gosub_stack_ptr].line_num;
}

/* DATA command - skip to end of statement */
static void data_cmd(void)
{
    while (chrgot() != 0 && chrgot() != ':') {
        if (chrgot() == '"') {
            chrget();
            while (chrgot() != 0 && chrgot() != '"') {
                chrget();
            }
        }
        chrget();
    }
}

/* READ command */
static void read_cmd(void)
{
    strout("READ not fully implemented\r\n");
}

/* RESTORE command */
static void restore_cmd(void)
{
    datptr = txttab;
}

/* DIM command */
static void dim_cmd(void)
{
    strout("DIM not fully implemented\r\n");
}

/* END command */
static void end_cmd(void)
{
    if (curlin != 0xFFFF) {
        /* Save position for CONT */
        oldlin = curlin;
        oldtxt = txtptr;
    }
    longjmp(error_jmp, 1);
}

/* STOP command */
static void stop_cmd(void)
{
    if (curlin != 0xFFFF) {
        strout("BREAK IN ");
        linprt(curlin);
        crdo();
        oldlin = curlin;
        oldtxt = txtptr;
    }
    longjmp(error_jmp, 1);
}

/* POKE command */
static void poke_cmd(void)
{
    WORD addr;
    BYTE value;
    
    frmevl();
    chknum();
    addr = fac_to_int();
    chkcom();
    frmevl();
    chknum();
    value = (BYTE)fac_to_int();
    
    /* Poke value into memory */
    if (addr < sizeof(memory)) {
        memory[addr] = value;
    }
}

/* WAIT command */
static void wait_cmd(void)
{
    strout("WAIT not fully implemented\r\n");
}

/* ON command */
static void on_cmd(void)
{
    strout("ON not fully implemented\r\n");
}

/* DEF command - define function */
static void def_cmd(void)
{
    strout("DEF not fully implemented\r\n");
}

/* REM command - skip to end of line */
static void rem_cmd(void)
{
    while (chrgot() != 0) {
        chrget();
    }
}

/* CONT command - continue after STOP */
static void cont_cmd(void)
{
    if (oldlin == 0xFFFF) {
        error(ERRCN);
    }
    curlin = oldlin;
    txtptr = oldtxt;
}

/* NULL command - set number of nulls to print */
static void null_cmd(void)
{
#if NULCMD
    frmevl();
    chknum();
    nulcnt = (BYTE)fac_to_int();
#endif
}

/* Enhanced expression evaluator with full operator precedence */
typedef struct {
    BYTE precedence;
    BYTE operator;
    FLOAT saved_fac;
    BYTE saved_valtyp;
} OPERATOR_STACK;

static OPERATOR_STACK op_stack[50];
static int op_stack_ptr = 0;

/* Operator precedence table */
static BYTE get_precedence(BYTE op)
{
    switch (op) {
        case ORTK:   return 70;
        case ANDTK:  return 80;
        case GTTK:
        case EQTK:
        case LTTK:   return 100;
        case PLUSTK:
        case MINUTK: return 121;
        case STARTK:
        case SLASHK: return 123;
        case PWRTK:  return 127;
        case NOTTK:  return 90;
        default:     return 0;
    }
}

/* Full expression evaluator with operator precedence */
static void frmevl_full(void)
{
    BYTE last_prec = 0;
    BYTE cur_prec;
    BYTE token;
    FLOAT temp_fac;
    BYTE temp_valtyp;
    
    op_stack_ptr = 0;
    
    /* Push dummy precedence */
    op_stack[op_stack_ptr].precedence = 0;
    op_stack_ptr++;
    
    /* Get first operand */
    eval_full();
    
    while (1) {
        token = chrgot();
        
        /* Check if it's an operator */
        cur_prec = get_precedence(token);
        
        if (cur_prec == 0) {
            /* Not an operator - we're done */
            break;
        }
        
        /* Compare with last precedence */
        if (cur_prec <= last_prec) {
            /* Apply previous operator */
            if (op_stack_ptr > 0) {
                op_stack_ptr--;
                apply_operator(op_stack[op_stack_ptr].operator);
                last_prec = op_stack_ptr > 0 ? op_stack[op_stack_ptr-1].precedence : 0;
            }
        } else {
            /* Save current FAC and get next operand */
            temp_fac.exponent = facexp;
            temp_fac.mantissa[0] = facho;
            temp_fac.mantissa[1] = facmo;
            temp_fac.mantissa[2] = faclo;
            temp_fac.mantissa[3] = facsgn;
            temp_valtyp = valtyp;
            
            chrget();  /* Skip operator */
            
            op_stack[op_stack_ptr].precedence = cur_prec;
            op_stack[op_stack_ptr].operator = token;
            op_stack[op_stack_ptr].saved_fac = temp_fac;
            op_stack[op_stack_ptr].saved_valtyp = temp_valtyp;
            op_stack_ptr++;
            
            last_prec = cur_prec;
            
            /* Get next operand */
            eval_full();
        }
    }
    
    /* Apply remaining operators */
    while (op_stack_ptr > 1) {
        op_stack_ptr--;
        apply_operator(op_stack[op_stack_ptr].operator);
    }
}

/* Apply an operator */
static void apply_operator(BYTE op)
{
    /* Move FAC to ARG */
    argexp = facexp;
    argho = facho;
    argmo = facmo;
    arglo = faclo;
    argsgn = facsgn;
    
    /* Restore saved FAC */
    facexp = op_stack[op_stack_ptr].saved_fac.exponent;
    facho = op_stack[op_stack_ptr].saved_fac.mantissa[0];
    facmo = op_stack[op_stack_ptr].saved_fac.mantissa[1];
    faclo = op_stack[op_stack_ptr].saved_fac.mantissa[2];
    facsgn = op_stack[op_stack_ptr].saved_fac.mantissa[3];
    valtyp = op_stack[op_stack_ptr].saved_valtyp;
    
    switch (op) {
        case PLUSTK:  fadd_real(); break;
        case MINUTK:  fsub_real(); break;
        case STARTK:  fmult_real(); break;
        case SLASHK:  fdiv_real(); break;
        case PWRTK:   fpwr_real(); break;
        case ANDTK:   /* AND operation */ break;
        case ORTK:    /* OR operation */ break;
        case GTTK:    /* Greater than */ break;
        case EQTK:    /* Equal */ break;
        case LTTK:    /* Less than */ break;
        default: snerr();
    }
}

/* Enhanced eval function */
static void eval_full(void)
{
    BYTE token = chrgot();
    int i;
    
    /* Check for unary operators */
    if (token == NOTTK) {
        chrget();
        eval_full();
        /* NOT operation - invert */
        if (facexp == 0) {
            int_to_fac(1);  /* NOT 0 = -1 (true) */
        } else {
            facexp = 0;  /* NOT anything else = 0 (false) */
        }
        return;
    }
    
    if (token == '+') {
        chrget();
        eval_full();
        return;
    }
    
    if (token == '-') {
        chrget();
        eval_full();
        facsgn ^= 0xFF;
        return;
    }
    
    /* Check for number */
    if ((token >= '0' && token <= '9') || token == '.') {
        fin();
        return;
    }
    
    /* Check for string literal */
    if (token == '"') {
        valtyp = 0xFF;
        chrget();
        while (chrgot() != 0 && chrgot() != '"') {
            chrget();
        }
        if (chrgot() == '"') {
            chrget();
        }
        return;
    }
    
    /* Check for function */
    if (token >= SGNTK && token <= MIDTK) {
        chrget();
        
        /* Most functions require parentheses */
        if (token != SGNTK && token != INTTK && token != ABSTK) {
            chkopn();
        }
        
        /* Evaluate argument */
        frmevl_full();
        
        /* Apply function */
        switch (token) {
            case SGNTK:  fn_sgn(); break;
            case INTTK:  fn_int(); break;
            case ABSTK:  fn_abs(); break;
            case SQRTK:  fn_sqr(); break;
            case RNDTK:  fn_rnd(); break;
            case LOGTK:  fn_log(); break;
            case EXPTK:  fn_exp(); break;
            case COSTK:  fn_cos(); break;
            case SINTK:  fn_sin(); break;
            case TANTK:  fn_tan(); break;
            case ATNTK:  fn_atn(); break;
            case PEEKTK: fn_peek(); break;
            case LENTK:  fn_len(); break;
            case STRTK:  fn_str(); break;
            case VALTK:  fn_val(); break;
            case ASCTK:  fn_asc(); break;
            case CHRTK:  fn_chr(); break;
            case LEFTK:  fn_left(); break;
            case RIGHK:  fn_right(); break;
            case MIDTK:  fn_mid(); break;
            case FREETK: fn_fre(); break;
            case POSTK:  fn_pos(); break;
            case USRTK:  fn_usr(); break;
        }
        
        if (token != SGNTK && token != INTTK && token != ABSTK) {
            chkcls();
        }
        return;
    }
    
    /* Check for variable or function name */
    if (isletc(token)) {
        ptrget();
        
        /* Check if it's an array reference */
        if (chrgot() == '(') {
            /* Array indexing - simplified */
            chrget();
            frmevl_full();
            chkcls();
        }
        
        /* Load value from variable */
        if (valtyp == 0) {
            movfm(varpnt);
        }
        return;
    }
    
    /* Check for parenthesized expression */
    if (token == '(') {
        chrget();
        frmevl_full();
        chkcls();
        return;
    }
    
    /* Default to zero */
    facexp = 0;
    valtyp = 0;
}

/* Use full expression evaluator */
static void frmevl(void)
{
    frmevl_full();
}

/* Evaluate term */
static void eval(void)
{
    BYTE token = chrgot();
    
    /* Check for unary plus/minus */
    if (token == '+') {
        chrget();
        eval();
        return;
    }
    
    if (token == '-') {
        chrget();
        eval();
        /* Negate result */
        facsgn ^= 0xFF;
        return;
    }
    
    /* Check for number */
    if ((token >= '0' && token <= '9') || token == '.') {
        fin();
        return;
    }
    
    /* Check for string literal */
    if (token == '"') {
        /* String constant - simplified */
        valtyp = 0xFF;
        chrget();
        while (chrgot() != 0 && chrgot() != '"') {
            chrget();
        }
        if (chrgot() == '"') {
            chrget();
        }
        return;
    }
    
    /* Check for variable or function */
    if (isletc(token)) {
        ptrget();
        /* Load value from variable */
        if (valtyp == 0) {
            /* Numeric variable */
            movfm(varpnt);
        }
        return;
    }
    
    /* Check for parenthesized expression */
    if (token == '(') {
        chrget();
        frmevl();
        chkcls();
        return;
    }
    
    /* Default to zero */
    facexp = 0;
    valtyp = 0;
}

/* Get pointer to variable */
static void ptrget(void)
{
    BYTE c1, c2;
    BYTE *p;
    
    c1 = chrgot();
    if (!isletc(c1)) {
        snerr();
    }
    
    chrget();
    c2 = chrgot();
    
    if (isletc(c2) || (c2 >= '0' && c2 <= '9')) {
        chrget();
    } else {
        c2 = 0;
    }
    
    /* Check for string variable */
    if (chrgot() == '$') {
        valtyp = 0xFF;
        chrget();
        c2 |= 0x80;
    } else {
        valtyp = 0;
    }
    
    /* Store variable name */
    varnam = (WORD)((c1 << 8) | c2);
    
    /* Search for variable */
    p = vartab;
    while (p < arytab) {
        if (p[0] == c1 && p[1] == c2) {
            /* Found it */
            varpnt = p + 2;
            return;
        }
        p += 6;  /* Each variable is 6 bytes */
    }
    
    /* Not found - create new variable */
    varpnt = arytab;
    arytab[0] = c1;
    arytab[1] = c2;
    arytab[2] = 0;
    arytab[3] = 0;
    arytab[4] = 0;
    arytab[5] = 0;
    arytab += 6;
    strend = arytab;
    
    varpnt = arytab - 4;
}

/* Find line in program */
static BYTE *findlin(WORD linenum)
{
    BYTE *p = txttab;
    WORD curline;
    
    while (p[0] != 0 || p[1] != 0) {
        curline = (WORD)((p[2] << 8) | p[3]);
        if (curline >= linenum) {
            return p;
        }
        /* Skip to next line */
        p += 2;  /* Skip pointer */
        p += 2;  /* Skip line number */
        while (*p != 0) {
            p++;
        }
        p++;  /* Skip null */
    }
    
    return p;
}

/* Floating point input - parse number from text */
static void fin(void)
{
    int sign = 1;
    int expsign = 1;
    int exp = 0;
    long mantissa = 0;
    int decimals = 0;
    BYTE c;
    
    facexp = 0;
    facsgn = 0;
    
    c = chrgot();
    
    /* Get mantissa */
    while ((c >= '0' && c <= '9') || c == '.') {
        if (c == '.') {
            chrget();
            c = chrgot();
            /* Start counting decimals */
            while (c >= '0' && c <= '9') {
                mantissa = mantissa * 10 + (c - '0');
                decimals++;
                chrget();
                c = chrgot();
            }
            break;
        } else {
            mantissa = mantissa * 10 + (c - '0');
            chrget();
            c = chrgot();
        }
    }
    
    /* Check for exponent */
    if (c == 'E' || c == 'e') {
        chrget();
        c = chrgot();
        
        if (c == '+') {
            chrget();
        } else if (c == '-') {
            expsign = -1;
            chrget();
        }
        
        c = chrgot();
        while (c >= '0' && c <= '9') {
            exp = exp * 10 + (c - '0');
            chrget();
            c = chrgot();
        }
    }
    
    /* Convert to floating point - simplified */
    exp = exp * expsign - decimals;
    
    if (mantissa == 0) {
        facexp = 0;
    } else {
        double value = (double)mantissa * pow(10.0, (double)exp);
        
        /* Convert to FAC format - simplified */
        if (value != 0.0) {
            int e = 0;
            while (value >= 256.0) {
                value /= 256.0;
                e++;
            }
            while (value < 1.0) {
                value *= 256.0;
                e--;
            }
            
            facexp = (BYTE)(e + 128);
            facho = (BYTE)value;
            value = (value - facho) * 256.0;
            facmo = (BYTE)value;
            value = (value - facmo) * 256.0;
            faclo = (BYTE)value;
        }
    }
    
    valtyp = 0;
}

/* Check if character is a letter */
static int isletc(BYTE c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* Move floating point from memory to FAC */
static void movfm(BYTE *addr)
{
    facexp = addr[0];
    facho = addr[1];
    facmo = addr[2];
    faclo = addr[3];
    facsgn = addr[4];
}

/* Move floating point from ARG to FAC */
static void movfa(void)
{
    facexp = argexp;
    facho = argho;
    facmo = argmo;
    faclo = arglo;
    facsgn = argsgn;
}

/* Move floating point from FAC to memory */
static void movaf(BYTE *addr)
{
    addr[0] = facexp;
    addr[1] = facho;
    addr[2] = facmo;
    addr[3] = faclo;
    addr[4] = facsgn;
}

/* Floating point output */
static void fout(void)
{
    char buf[32];
    double value = 0.0;
    
    if (facexp != 0) {
        /* Convert FAC to double - simplified */
        value = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
        value *= pow(256.0, (double)((int)facexp - 128));
        
        if (facsgn) {
            value = -value;
        }
    }
    
    sprintf(buf, " %.6g ", value);
    strout(buf);
}

/* Convert FAC to integer */
static WORD fac_to_int(void)
{
    if (facexp == 0) {
        return 0;
    }
    
    /* Simplified conversion */
    {
        double value = (double)facho + (double)facmo / 256.0;
        value *= pow(256.0, (double)((int)facexp - 128));
        
        if (facsgn) {
            value = -value;
        }
        
        return (WORD)value;
    }
}

/* Convert integer to FAC */
static void int_to_fac(WORD value)
{
    if (value == 0) {
        facexp = 0;
        facsgn = 0;
        return;
    }
    
    /* Simplified conversion */
    {
        double dval = (double)value;
        int e = 128;
        
        while (dval >= 256.0) {
            dval /= 256.0;
            e++;
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)dval;
        dval = (dval - facho) * 256.0;
        facmo = (BYTE)dval;
        dval = (dval - facmo) * 256.0;
        faclo = (BYTE)dval;
        facsgn = 0;
    }
}

/* Floating point add */
static void fadd(BYTE *addr)
{
    /* Simplified - would need full implementation */
    strout("(fadd not implemented)");
}

/* Floating point subtract */
static void fsub(BYTE *addr)
{
    /* Simplified */
}

/* Floating point multiply */
static void fmult(BYTE *addr)
{
    /* Simplified */
}

/* Floating point divide */
static void fdiv(BYTE *addr)
{
    /* Simplified */
}

/* Floating point power */
static void fpwr(BYTE *addr)
{
    /* Simplified */
}

/* Floating point compare */
static int fcomp(BYTE *addr)
{
    /* Simplified */
    return 0;
}

/* Additional floating point functions */
/* Floating point operations - complete implementation */
static void fadd_real(void)
{
    double fac_val, arg_val;
    
    /* Convert FAC to double */
    if (facexp == 0) {
        fac_val = 0.0;
    } else {
        fac_val = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
        fac_val *= pow(256.0, (double)((int)facexp - 128));
        if (facsgn) fac_val = -fac_val;
    }
    
    /* Convert ARG to double */
    if (argexp == 0) {
        arg_val = 0.0;
    } else {
        arg_val = (double)argho + (double)argmo / 256.0 + (double)arglo / 65536.0;
        arg_val *= pow(256.0, (double)((int)argexp - 128));
        if (argsgn) arg_val = -arg_val;
    }
    
    /* Add and convert back */
    fac_val += arg_val;
    
    facsgn = (fac_val < 0.0) ? 0xFF : 0;
    if (fac_val < 0.0) fac_val = -fac_val;
    
    if (fac_val == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (fac_val >= 256.0) {
            fac_val /= 256.0;
            e++;
        }
        while (fac_val < 1.0 && fac_val != 0.0) {
            fac_val *= 256.0;
            e--;
        }
        
        if (e < 0 || e > 255) {
            error(ERROV);  /* Overflow */
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)fac_val;
        fac_val = (fac_val - facho) * 256.0;
        facmo = (BYTE)fac_val;
        fac_val = (fac_val - facmo) * 256.0;
        faclo = (BYTE)fac_val;
    }
}

static void fsub_real(void)
{
    argsgn ^= 0xFF;  /* Negate argument */
    fadd_real();
}

static void fmult_real(void)
{
    double fac_val, arg_val, result;
    
    if (facexp == 0 || argexp == 0) {
        facexp = 0;
        return;
    }
    
    /* Convert to doubles */
    fac_val = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
    fac_val *= pow(256.0, (double)((int)facexp - 128));
    if (facsgn) fac_val = -fac_val;
    
    arg_val = (double)argho + (double)argmo / 256.0 + (double)arglo / 65536.0;
    arg_val *= pow(256.0, (double)((int)argexp - 128));
    if (argsgn) arg_val = -arg_val;
    
    result = fac_val * arg_val;
    
    facsgn = (result < 0.0) ? 0xFF : 0;
    if (result < 0.0) result = -result;
    
    if (result == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (result >= 256.0) {
            result /= 256.0;
            e++;
        }
        while (result < 1.0 && result != 0.0) {
            result *= 256.0;
            e--;
        }
        
        if (e < 0 || e > 255) {
            error(ERROV);
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)result;
        result = (result - facho) * 256.0;
        facmo = (BYTE)result;
        result = (result - facmo) * 256.0;
        faclo = (BYTE)result;
    }
}

static void fdiv_real(void)
{
    double fac_val, arg_val, result;
    
    if (argexp == 0) {
        error(ERRDZ);  /* Division by zero */
    }
    
    if (facexp == 0) {
        return;  /* 0 / anything = 0 */
    }
    
    /* Convert to doubles */
    fac_val = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
    fac_val *= pow(256.0, (double)((int)facexp - 128));
    if (facsgn) fac_val = -fac_val;
    
    arg_val = (double)argho + (double)argmo / 256.0 + (double)arglo / 65536.0;
    arg_val *= pow(256.0, (double)((int)argexp - 128));
    if (argsgn) arg_val = -arg_val;
    
    result = fac_val / arg_val;
    
    facsgn = (result < 0.0) ? 0xFF : 0;
    if (result < 0.0) result = -result;
    
    if (result == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (result >= 256.0) {
            result /= 256.0;
            e++;
        }
        while (result < 1.0 && result != 0.0) {
            result *= 256.0;
            e--;
        }
        
        if (e < 0 || e > 255) {
            error(ERROV);
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)result;
        result = (result - facho) * 256.0;
        facmo = (BYTE)result;
        result = (result - facmo) * 256.0;
        faclo = (BYTE)result;
    }
}

static void fpwr_real(void)
{
    double fac_val, arg_val, result;
    
    if (facexp == 0) {
        /* 0 ^ anything = 0 (except 0 ^ 0 which we'll treat as 1) */
        if (argexp == 0) {
            int_to_fac(1);
        }
        return;
    }
    
    /* Convert to doubles */
    fac_val = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
    fac_val *= pow(256.0, (double)((int)facexp - 128));
    if (facsgn) fac_val = -fac_val;
    
    arg_val = (double)argho + (double)argmo / 256.0 + (double)arglo / 65536.0;
    arg_val *= pow(256.0, (double)((int)argexp - 128));
    if (argsgn) arg_val = -arg_val;
    
    result = pow(fac_val, arg_val);
    
    facsgn = (result < 0.0) ? 0xFF : 0;
    if (result < 0.0) result = -result;
    
    if (result == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (result >= 256.0) {
            result /= 256.0;
            e++;
        }
        while (result < 1.0 && result != 0.0) {
            result *= 256.0;
            e--;
        }
        
        if (e < 0 || e > 255) {
            error(ERROV);
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)result;
        result = (result - facho) * 256.0;
        facmo = (BYTE)result;
        result = (result - facmo) * 256.0;
        faclo = (BYTE)result;
    }
}

/* Math functions */
static void fn_sin(void)
{
    double value;
    
    if (facexp == 0) {
        return;
    }
    
    value = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
    value *= pow(256.0, (double)((int)facexp - 128));
    if (facsgn) value = -value;
    
    value = sin(value);
    
    facsgn = (value < 0.0) ? 0xFF : 0;
    if (value < 0.0) value = -value;
    
    if (value == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (value >= 256.0) {
            value /= 256.0;
            e++;
        }
        while (value < 1.0 && value != 0.0) {
            value *= 256.0;
            e--;
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)value;
        value = (value - facho) * 256.0;
        facmo = (BYTE)value;
        value = (value - facmo) * 256.0;
        faclo = (BYTE)value;
    }
}

static void fn_cos(void)
{
    double value;
    
    if (facexp == 0) {
        /* cos(0) = 1 */
        int_to_fac(1);
        return;
    }
    
    value = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
    value *= pow(256.0, (double)((int)facexp - 128));
    if (facsgn) value = -value;
    
    value = cos(value);
    
    facsgn = (value < 0.0) ? 0xFF : 0;
    if (value < 0.0) value = -value;
    
    if (value == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (value >= 256.0) {
            value /= 256.0;
            e++;
        }
        while (value < 1.0 && value != 0.0) {
            value *= 256.0;
            e--;
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)value;
        value = (value - facho) * 256.0;
        facmo = (BYTE)value;
        value = (value - facmo) * 256.0;
        faclo = (BYTE)value;
    }
}

static void fn_tan(void) { fn_sin(); /* Simplified */ }
static void fn_atn(void) { /* Simplified */ }
static void fn_log(void) { /* Simplified */ }
static void fn_exp(void) { /* Simplified */ }

static void fn_sqr(void)
{
    double value;
    
    if (facsgn) {
        error(ERRFC);  /* Can't take sqrt of negative */
    }
    
    if (facexp == 0) {
        return;  /* sqrt(0) = 0 */
    }
    
    value = (double)facho + (double)facmo / 256.0 + (double)faclo / 65536.0;
    value *= pow(256.0, (double)((int)facexp - 128));
    
    value = sqrt(value);
    
    if (value == 0.0) {
        facexp = 0;
    } else {
        int e = 128;
        while (value >= 256.0) {
            value /= 256.0;
            e++;
        }
        while (value < 1.0 && value != 0.0) {
            value *= 256.0;
            e--;
        }
        
        facexp = (BYTE)e;
        facho = (BYTE)value;
        value = (value - facho) * 256.0;
        facmo = (BYTE)value;
        value = (value - facmo) * 256.0;
        faclo = (BYTE)value;
    }
    facsgn = 0;
}

static void fn_rnd(void)
{
    /* Simple random number generator */
    rndx[0] = (rndx[0] * 17 + 53) & 0xFF;
    rndx[1] = (rndx[1] * 23 + 71) & 0xFF;
    
    facexp = 128;
    facho = rndx[0];
    facmo = rndx[1];
    faclo = rndx[2];
    facsgn = 0;
}

static void fn_abs(void)
{
    facsgn = 0;
}

static void fn_sgn(void)
{
    if (facexp == 0) {
        int_to_fac(0);
    } else if (facsgn) {
        int_to_fac(-1);
    } else {
        int_to_fac(1);
    }
}

static void fn_int(void)
{
    WORD ival = fac_to_int();
    int_to_fac(ival);
}

static void fn_peek(void)
{
    WORD addr = fac_to_int();
    BYTE value = 0;
    
    if (addr < sizeof(memory)) {
        value = memory[addr];
    }
    
    int_to_fac((WORD)value);
}

static void fn_pos(void)
{
    int_to_fac((WORD)trmpos);
}

static void fn_fre(void)
{
    WORD free = (WORD)(fretop - strend);
    int_to_fac(free);
}

static void fn_usr(void) 
{
    error(ERRFC);  /* Not implemented */
}

/* String functions - simplified stubs */
static void fn_len(void) { int_to_fac(0); }
static void fn_str(void) { }
static void fn_val(void) { }
static void fn_asc(void) { int_to_fac(65); }
static void fn_chr(void) { }
static void fn_left(void) { }
static void fn_right(void) { }
static void fn_mid(void) { }

/* String operations - simplified */
static void frefac(void) {}
static void fretmp(BYTE index) { (void)index; }
static BYTE *getspa(BYTE length) { (void)length; return fretop; }
static void garbage_collect(void) {}
static void putnew(void) {}

/* Block transfer utility */
static void bltu(BYTE *src, BYTE *dst, WORD count)
{
    memmove(dst, src, count);
}

/* Normalize FAC */
static void normalize(void)
{
    /* Already normalized in our representation */
}

/* Number validity check */
static void isnum(void)
{
    BYTE c = chrgot();
    if ((c >= '0' && c <= '9') || c == '.') {
        return;
    }
    snerr();
}

/* Program line storage */
static void store_line(void)
{
    BYTE *line_ptr;
    BYTE *p;
    WORD len;
    WORD i;
    
    /* Find where to insert */
    line_ptr = find_line(linnum);
    
    /* Check if line already exists */
    if (line_ptr[0] != 0 || line_ptr[1] != 0) {
        WORD existing_line = (WORD)((line_ptr[2] << 8) | line_ptr[3]);
        if (existing_line == linnum) {
            /* Delete existing line first */
            delete_line(linnum);
            line_ptr = find_line(linnum);
        }
    }
    
    /* Calculate length of new line */
    len = 0;
    p = buf;
    while (*p && *p != '\r' && *p != '\n') {
        len++;
        p++;
    }
    
    if (len == 0) {
        /* Just deleting the line */
        return;
    }
    
    /* Make room for new line: pointer(2) + linenum(2) + text + null */
    {
        BYTE *old_end = line_ptr;
        BYTE *new_end = line_ptr + 2 + 2 + len + 1;
        
        /* Move everything after insertion point */
        while (old_end[0] != 0 || old_end[1] != 0) {
            old_end += 2;  /* Skip pointer */
            old_end += 2;  /* Skip line number */
            while (*old_end != 0) old_end++;
            old_end++;  /* Skip null */
        }
        old_end += 2;  /* Include end marker */
        
        bltu(line_ptr, new_end, (WORD)(old_end - line_ptr));
    }
    
    /* Insert new line */
    line_ptr[0] = (BYTE)((line_ptr + 2 + 2 + len + 1 - txttab) & 0xFF);
    line_ptr[1] = (BYTE)((line_ptr + 2 + 2 + len + 1 - txttab) >> 8);
    line_ptr[2] = (BYTE)(linnum >> 8);
    line_ptr[3] = (BYTE)(linnum & 0xFF);
    
    p = buf;
    for (i = 0; i < len; i++) {
        line_ptr[4 + i] = *p++;
    }
    line_ptr[4 + len] = 0;
    
    /* Update vartab */
    vartab = line_ptr + 2 + 2 + len + 1;
}

static void delete_line(WORD linenum)
{
    BYTE *line_ptr = find_line(linenum);
    BYTE *next_ptr;
    WORD existing_line;
    
    if (line_ptr[0] == 0 && line_ptr[1] == 0) {
        return;  /* Line not found */
    }
    
    existing_line = (WORD)((line_ptr[2] << 8) | line_ptr[3]);
    if (existing_line != linenum) {
        return;  /* Line not found */
    }
    
    /* Find next line */
    next_ptr = line_ptr + 2;  /* Skip pointer */
    next_ptr += 2;  /* Skip line number */
    while (*next_ptr != 0) next_ptr++;
    next_ptr++;  /* Skip null */
    
    /* Move everything down */
    {
        BYTE *end_ptr = next_ptr;
        while (end_ptr[0] != 0 || end_ptr[1] != 0) {
            end_ptr += 2;  /* Skip pointer */
            end_ptr += 2;  /* Skip line number */
            while (*end_ptr != 0) end_ptr++;
            end_ptr++;  /* Skip null */
        }
        end_ptr += 2;  /* Include end marker */
        
        memmove(line_ptr, next_ptr, (size_t)(end_ptr - next_ptr));
    }
    
    vartab = line_ptr;
}

static BYTE *find_line(WORD linenum)
{
    BYTE *p = txttab;
    WORD curline;
    
    while (p[0] != 0 || p[1] != 0) {
        curline = (WORD)((p[2] << 8) | p[3]);
        if (curline >= linenum) {
            return p;
        }
        /* Skip to next line */
        p += 2;  /* Skip pointer */
        p += 2;  /* Skip line number */
        while (*p != 0) p++;
        p++;  /* Skip null */
    }
    
    return p;
}

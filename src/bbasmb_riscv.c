/**************************************************************** \
*       32-bit BBC BASIC Interpreter                              *
*       (c) 2018-2021  R.T.Russell  http://www.rtrussell.co.uk/   *
*                                                                 *
*       bbasmb.c: Simple RISC-V assembler                         *
*       Version 1.24a, 12-Jul-2021                                *
\*****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "BBC.h"

#ifndef __WINDOWS__
#define stricmp strcasecmp
#define strnicmp strncasecmp
#endif

#if defined(__x86_64__) || defined(__aarch64__)
#define OC ((unsigned int) stavar[15] + (void *)((long long) stavar[13] << 32))
#define PC ((unsigned int) stavar[16] + (void *)((long long) stavar[17] << 32))
#else
#define OC (void *) stavar[15]
#define PC (void *) stavar[16]
#endif

// External routines:
void newlin (void) ;
void *getput (unsigned char *) ;
void error (int, const char *) ;
void token (signed char) ;
void text (const char*) ;
void crlf (void) ;
void comma (void) ;
void spaces (int) ;
int range0 (char) ;
signed char nxt (void) ;

long long itemi (void) ;
long long expri (void) ;
VAR expr (void) ;
VAR exprs (void) ;
VAR loadn (void *, unsigned char) ;
void storen (VAR, void *, unsigned char) ;

// Special value for the x#<itemi> reg name prefix
#define XHASH 32

// Opcode register positions within 32-bit instruction

#define RD   7
#define RS1 15
#define RS2 20

// Opcode constant definitions
// See:

#define OP_ADD       0x00000033
#define OP_SUB       0x40000033
#define OP_XOR       0x00004033
#define OP_OR        0x00006033
#define OP_AND       0x00007033
#define OP_SLL       0x00001033
#define OP_SRL       0x00005033
#define OP_SRA       0x40005033
#define OP_SLT       0x00002033
#define OP_SLTU      0x00003033

#define OP_ADDI      0x00000013
#define OP_XORI      0x00004013
#define OP_ORI       0x00006013
#define OP_ANDI      0x00007013
#define OP_SLLI      0x00001013
#define OP_SRLI      0x00005013
#define OP_SRAI      0x40005013
#define OP_SLTI      0x00002013
#define OP_SLTUI     0x00003013

#define OP_LB        0x00000003
#define OP_LH        0x00001003
#define OP_LW        0x00002003
#define OP_LBU       0x00004003
#define OP_LHU       0x00005003

#define OP_SB        0x00000023
#define OP_SH        0x00001023
#define OP_SW        0x00002023

#define OP_BEQ       0x00000063
#define OP_BNE       0x00001063
#define OP_BLT       0x00004063
#define OP_BGE       0x00005063
#define OP_BLTU      0x00006063
#define OP_BGEU      0x00007063

#define OP_JAL       0x0000006f
#define OP_JALR      0x00000067

#define OP_LUI       0x00000037
#define OP_AUIPC     0x00000017

#define OP_ECALL     0x00000073
#define OP_EBREAK    0x00100073

#define OP_MULH      0x02001033
#define OP_MULSU     0x02002033
#define OP_MULU      0x02003033
#define OP_MUL       0x02000033
#define OP_DIVU      0x02005033
#define OP_DIV       0x02004033
#define OP_REMU      0x02007033
#define OP_REM       0x02006033

#define OP_CSRRW     0x00001073
#define OP_CSRRS     0x00002073
#define OP_CSRRC     0x00003073
#define OP_CSRRWI    0x00005073
#define OP_CSRRSI    0x00006073
#define OP_CSRRCI    0x00007073

#define OP_MRET      0x30200073
#define OP_SRET      0x10200073
#define OP_URET      0x00200073
#define OP_WFI       0x10500073

#define OP_BUILTIN   0xffffffff
#define OP_PSEUDO    0x00000000

// Flags support the efficient implementation of many of the pseudo
// instructions. They are overloaded onto the 32-bit opcode in 31 and
// 19..16 bits which are normally zero.

#define F_REVERSED   0x80000000 // reverse rs1 and rs2
#define F_ZERO       0x00080000 // force a zero into rs2 (or rs1 if reversed)
#define F_IMM12_000  0x00040000 // force imm12 to 000
#define F_IMM12_001  0x00020000 // force imm12 to 001
#define F_IMM12_FFF  0x00010000 // force imm12 to fff

#define F_CLEAR_MASK 0x7FF0FFFF

static char *mnemonics[] = {
   "addi",
   "add",
   "align",
   "andi",
   "and",
   "auipc",
   "beqz",
   "beq",
   "bgeu",
   "bgez",
   "bge",
   "bgtu",
   "bgtz",
   "bgt",
   "bleu",
   "blez",
   "ble",
   "bltu",
   "bltz",
   "blt",
   "bnez",
   "bne",
   "call",
   "csrrci",
   "csrrc",
   "csrrsi",
   "csrrs",
   "csrrwi",
   "csrrw",
   "csrr",
   "csrwi",
   "csrw",
   "db",
   "dcb",
   "dcd",
   "dcs",
   "dcw",
   "divu",
   "div",
   "ebreak",
   "ecall",
   "equb",
   "equd",
   "equq",
   "equs",
   "equw",
   "jalr",
   "jal",
   "jr",
   "j",
   "la",
   "lbu",
   "lb",
   "lhu",
   "lh",
   "li",
   "lui",
   "lw",
   "mret",
   "mulh",
   "mulsu",
   "mulu",
   "mul",
   "mv",
   "neg",
   "nop",
   "not",
   "opt",
   "ori",
   "or",
   "remu",
   "rem",
   "ret",
   "sb",
   "seqz",
   "sgtz",
   "sh",
   "slli",
   "sll",
   "slti",
   "sltui",
   "sltu",
   "sltz",
   "slt",
   "snze",
   "srai",
   "sra",
   "sret",
   "srli",
   "srl",
   "sub",
   "sw",
   "tail",
   "uret",
   "wfi",
   "xori",
   "xor"
};


static uint32_t opcodes[] = {
   OP_ADDI,                         // addi
   OP_ADD,                          // add
   OP_BUILTIN,                      // align
   OP_ANDI,                         // andi
   OP_AND,                          // and
   OP_AUIPC,                        // auipc
   OP_BEQ | F_ZERO,                 // beqz
   OP_BEQ,                          // beq
   OP_BGEU,                         // bgeu
   OP_BGE | F_ZERO,                 // bgez
   OP_BGE,                          // bge
   OP_BLTU | F_REVERSED,            // bgtu    (bltu with rs1/2 reversed)
   OP_BLT  | F_REVERSED | F_ZERO,   // bgtz    (bltz with rs1/2 reversed)
   OP_BLT  | F_REVERSED,            // bgt     (blt  with rs1/2 reversed)
   OP_BGEU | F_REVERSED,            // bleu    (bgeu with rs1/2 reversed)
   OP_BGE  | F_REVERSED | F_ZERO,   // blez    (bgez with rs1/2 reversed)
   OP_BGE  | F_REVERSED,            // ble     (bge  with rs1/2 reversed)
   OP_BLTU,                         // bltu
   OP_BLT | F_ZERO,                 // bltz
   OP_BLT,                          // blt
   OP_BNE | F_ZERO,                 // bnez
   OP_BNE,                          // bne
   OP_PSEUDO,                       // call
   OP_CSRRCI,                       // csrrci
   OP_CSRRC ,                       // csrrc
   OP_CSRRSI,                       // csrrsi
   OP_CSRRS ,                       // csrrs
   OP_CSRRWI,                       // csrrwi
   OP_CSRRW ,                       // csrrw
   OP_CSRRS,                        // csrr    (csrrs    rd, csr, zero)
   OP_CSRRWI,                       // csrwi   (csrrwi zero, csr, rs1)
   OP_CSRRW,                        // csrw    (csrrwi zero, csr, uimm5)
   OP_BUILTIN,                      // db
   OP_BUILTIN,                      // dcb
   OP_BUILTIN,                      // dcd
   OP_BUILTIN,                      // dcs
   OP_BUILTIN,                      // dcw
   OP_DIVU,                         // divu
   OP_DIV,                          // div
   OP_EBREAK,                       // ebreak
   OP_ECALL,                        // ecall
   OP_BUILTIN,                      // equb
   OP_BUILTIN,                      // equd
   OP_BUILTIN,                      // equq
   OP_BUILTIN,                      // equs
   OP_BUILTIN,                      // equw
   OP_JALR,                         // jalr
   OP_JAL,                          // jal
   OP_JALR,                         // jr      (jalr zero, rs, offset)
   OP_JAL,                          // j       (jal zero, offset)
   OP_PSEUDO,                       // la      (pseudo)
   OP_LBU,                          // lbu
   OP_LB,                           // lb
   OP_LHU,                          // lhu
   OP_LH,                           // lh
   OP_PSEUDO,                       // li      (pseudo)
   OP_LUI,                          // lui
   OP_LW,                           // lw
   OP_MRET,                         // mret
   OP_MULH,                         // mulh
   OP_MULSU,                        // mulsu
   OP_MULU,                         // mulu
   OP_MUL,                          // mul
   OP_ADDI | F_IMM12_000,           // mv      (addi rd, rs, 0)
   OP_SUB | F_REVERSED | F_ZERO,    // neg     (sub  rd, zero, rs)
   OP_ADDI,                         // nop     (addi zero, zero, 0)
   OP_XORI | F_IMM12_FFF,           // not     (xori rd, rs, -1)
   OP_BUILTIN,                      // opt
   OP_ORI,                          // ori
   OP_OR,                           // or
   OP_REMU,                         // remu
   OP_REM,                          // rem
   OP_JALR | (1 << RS1),            // ret     (jalr zero, ra, 0)
   OP_SB,                           // sb
   OP_SLTUI | F_IMM12_001,          // seqz    (sltiu rd, rs, 1)
   OP_SLT | F_REVERSED | F_ZERO,    // sgtz    (slt   rd, zero, rs)
   OP_SH,                           // sh
   OP_SLLI,                         // slli
   OP_SLL,                          // sll
   OP_SLTI,                         // slti
   OP_SLTUI,                        // sltui
   OP_SLTU,                         // sltu
   OP_SLT | F_ZERO,                 // sltz    (slt rd, rs, zero)
   OP_SLT,                          // slt
   OP_SLTU | F_REVERSED | F_ZERO,   // snez    (sltu rd, zero, rs)
   OP_SRAI,                         // srai
   OP_SRA,                          // sra
   OP_SRET,                         // sret
   OP_SRLI,                         // srli
   OP_SRL,                          // srl
   OP_SUB,                          // sub
   OP_SW,                           // sw
   OP_PSEUDO,                       // tail
   OP_URET,                         // uret
   OP_WFI,                          // wfi
   OP_XORI,                         // xori
   OP_XOR                           // xor
};

enum {
   ADDI,
   ADD,
   ALIGN,
   ANDI,
   AND,
   AUIPC,
   BEQZ,
   BEQ,
   BGEU,
   BGEZ,
   BGE,
   BGTU,
   BGTZ,
   BGT,
   BLEU,
   BLEZ,
   BLE,
   BLTU,
   BLTZ,
   BLT,
   BNEZ,
   BNE,
   CALL,
   CSRRCI,
   CSRRC,
   CSRRSI,
   CSRRS,
   CSRRWI,
   CSRRW,
   CSRR,
   CSRWI,
   CSRW,
   DB,
   DCB,
   DCD,
   DCS,
   DCW,
   DIVU,
   DIV,
   EBREAK,
   ECALL,
   EQUB,
   EQUD,
   EQUQ,
   EQUS,
   EQUW,
   JALR,
   JAL,
   JR,
   J,
   LA,
   LBU,
   LB,
   LHU,
   LH,
   LI,
   LUI,
   LW,
   MRET,
   MULH,
   MULSU,
   MULU,
   MUL,
   MV,
   NEG,
   NOP,
   NOT,
   OPT,
   ORI,
   OR,
   REMU,
   REM,
   RET,
   SB,
   SEQZ,
   SGTZ,
   SH,
   SLLI,
   SLL,
   SLTI,
   SLTUI,
   SLTU,
   SLTZ,
   SLT,
   SNEZ,
   SRAI,
   SRA,
   SRET,
   SRLI,
   SRL,
   SUB,
   SW,
   TAIL,
   URET,
   WFI,
   XORI,
   XOR
};

static char *registers[] = {
   "a0",
   "a1",
   "a2",
   "a3",
   "a4",
   "a5",
   "a6",
   "a7",
   "gp",
   "ra",
   "s0",
   "s10",
   "s11",
   "s1",
   "s2",
   "s3",
   "s4",
   "s5",
   "s6",
   "s7",
   "s8",
   "s9",
   "sp",
   "t0",
   "t1",
   "t2",
   "t3",
   "t4",
   "t5",
   "t6",
   "tp",
   "x#",
   "x0",
   "x10",
   "x1",
   "x11",
   "x12",
   "x13",
   "x14",
   "x15",
   "x16",
   "x17",
   "x18",
   "x19",
   "x20",
   "x21",
   "x2",
   "x22",
   "x23",
   "x24",
   "x25",
   "x26",
   "x27",
   "x28",
   "x29",
   "x30",
   "x31",
   "x3",
   "x4",
   "x5",
   "x6",
   "x7",
   "x8",
   "x9",
   "zero"
};

static unsigned char regno[] = {
   10, // a0
   11, // a1
   12, // a2
   13, // a3
   14, // a4
   15, // a5
   16, // a6
   17, // a7
   3,  // gp
   1,  // ra
   8,  // s0
   26, // s10
   27, // s11
   9,  // s1
   18, // s2
   19, // s3
   20, // s4
   21, // s5
   22, // s6
   23, // s7
   24, // s8
   25, // s9
   2,  // sp
   5,  // t0
   6,  // t1
   7,  // t2
   28, // t3
   29, // t4
   30, // t5
   31, // t6
   4,  // tp
   XHASH, // x#
   0,  // x0
   10, // x10
   1,  // x1
   11, // x11
   12, // x12
   13, // x13
   14, // x14
   15, // x15
   16, // x16
   17, // x17
   18, // x18
   19, // x19
   20, // x20
   21, // x21
   2,  // x2
   22, // x22
   23, // x23
   24, // x24
   25, // x25
   26, // x26
   27, // x27
   28, // x28
   29, // x29
   30, // x30
   31, // x31
   3,  // x3
   4,  // x4
   5,  // x5
   6,  // x6
   7,  // x7
   8,  // x8
   9,  // x9
   0   // zero
};

static char *csrs[] = {
   "cycleh",
   "cycle",
   "instreth",
   "instret",
   "marchid",
   "mcause",
   "mcounteren",
   "medeleg",
   "mepc",
   "mhartid",
   "mideleg",
   "mie",
   "mimpid",
   "mip",
   "misa",
   "mscratch",
   "mstatus",
   "mtval",
   "mtvec",
   "mvendorid",
   "timeh",
   "time"
};

static int csrno[] = {
   0xc80, // cycleh
   0xc00, // cycle
   0xc82, // instreth
   0xc02, // instret
   0xf12, // marchid
   0x342, // mcause
   0x306, // mcounteren
   0x302, // medeleg
   0x341, // mepc
   0xf14, // mhartid
   0x303, // mideleg
   0x304, // mie
   0xf13, // mimpid
   0x344, // mip
   0x301, // misa
   0x340, // mscratch
   0x300, // mstatus
   0x343, // mtval
   0x305, // mtvec
   0xf11, // mvendorid
   0xc81, // timeh
   0xc01  // time
};

static int lookup (char **arr, int num)
{
   int i, n ;
   for (i = 0; i < num; i++)
      {
         n = strlen (*(arr + i)) ;
         if (strnicmp ((const char *)esi, *(arr + i), n) == 0)
            break ;
      }
   if (i >= num)
      return -1 ;
   esi += n ;
   return i ;
}

static int lookup_mnemonic (void) {
   const char *code = (const char *)esi;

   // Tokenized AND, ANDI

   if (code[0] == (char) TAND) {
      if (code[1] == 'i' || code[1] == 'I') {
         esi += 2;
         return ANDI;
      } else {
         esi += 1;
         return AND;
      }
   }

    // Tokenized OR, ORI
   if (code[0] == (char) TOR) {
      if (code[1] == 'i' || code[1] == 'I') {
         esi += 2;
         return ORI;
      } else {
         esi += 1;
         return OR;
      }
   }

   // Tokenized DIV, DIVU
   if (code[0] == (char) TDIV) {
      if (code[1] == 'u' || code[1] == 'U') {
         esi += 2;
         return DIVU;
      } else {
         esi += 1;
         return DIV;
      }
   }

   // Tokenized REM, REMU
   if (code[0] == (char) TREM) {
      if (code[1] == 'u' || code[1] == 'U') {
         esi += 2;
         return REMU;
      } else {
         esi += 1;
         return REM;
      }
   }

   // Tokenized NOT
   if (code[0] == (char) TNOT) {
      esi += 1;
      return NOT;
   }

   // Tokenized CALL
   if (code[0] == (char) TCALL) {
      esi += 1;
      return CALL;
   }

   return lookup(mnemonics, sizeof(mnemonics)/sizeof(mnemonics[0])) ;
}

// Parse a register without throwing an error
static int parse_reg () {
   nxt();
   int i = lookup (registers, sizeof(registers) / sizeof(registers[0])) ;
   if (i < 0) {
      return -1;
   }
   i = regno[i];
   // Handle the special x#<itemi> prefix
   if (i == XHASH) {
      i = itemi();
   }
   // Sanity check it's a valid register
   if (i >= 0 && i <= 31) {
      return i;
   } else {
      return -1;
   }
}

static int count_regs () {
   signed char *old_esi = esi; // save the program pointer
   int i, n = 0;
   do {
      i = parse_reg();
      if (i < 0) {
         break; // not a register, so terminate
      }
      n++;
      if (nxt() != ',') {
         break; // not a comma, so terminate
      }
      esi++; // skip over the comma
   } while (1);
   esi = old_esi; // restore the program pointer
   return n;
}

static unsigned char reg (void)
{
   int i = parse_reg();
   if (i < 0) {
      error (16, NULL) ; // 'Syntax error'
   }
   return (unsigned char) i;
}

static unsigned int csr (void)
{
   int i ;
   nxt () ;

   // Tokenized TIME, TIMEH

   const char *code = (const char *)esi;

   if (code[0] == (char) TTIMEL || code[0] == (char) TTIMER) {
      if (code[1] == 'h' || code[1] == 'H') {
         esi += 2;
         return 0xc81;
      } else {
         esi += 1;
         return 0xc01;
      }
   }

   i = lookup (csrs, sizeof(csrs) / sizeof(csrs[0])) ;
   if (i < 0)
      {
         i = itemi() ;
         if ((i < 0) || (i > 4095))
            error (16, NULL) ; // 'Syntax error'
         return i ;
      }
   return csrno[i] ;
}

static inline void validate_uimm5(long long immediate) {
   if (immediate < 0x00 || immediate >= 0x20) {
      error (17, "uimm5 constant out of range");
   }
}

static inline void validate_imm12(long long immediate) {
   if (immediate < -0x800 || immediate >= 0x800) {
      error (17, "imm12 constant out of range");
   }
}

static inline void validate_imm20(long long immediate) {
   if (immediate < -0x80000 || immediate >= 0x80000) {
      error (17, "imm20 constant out of range");
   }
}

static unsigned int uimm5(void) {
   long long immediate = expri();
   // immediate is a 5-bit unsigned
   validate_uimm5(immediate);
   return ((unsigned int) immediate) & 0x1F;
}

static unsigned int imm12(void) {
   long long immediate = expri();
   // immediate is a 12-bit signed
   validate_imm12(immediate);
   return ((unsigned int) immediate) & 0xFFF;
}

static unsigned int imm20(void) {
   long long immediate = expri();
   // immediate is a 20-bit signed
   validate_imm20(immediate);
   return ((unsigned int) immediate) & 0xFFFFF;
}

static void tabit (int x)
{
   if (vcount == x)
      return ;
   if (vcount > x)
      crlf () ;
   spaces (x - vcount) ;
}

static void poke (void *p, int n)
{
   char *d ;
   if (liston & BIT6)
      {
         d = OC ;
         stavar[15] += n ;
      }
   else
      d = PC ;

   stavar[16] += n ;
   memcpy (d, p, n) ;
}

static void *align (void)
{
   while (stavar[16] & 3)
      {
         stavar[16]++ ;
         if (liston & BIT6)
            stavar[15]++ ;
      } ;
   return PC ;
}

static int eol (char ch)
{
   return (( ch == 0x0D ) || ( ch == ':' ) || ( ch == ';' )) ? 1 : 0;
}

static unsigned int generate_auipc(int rd, long long target) {
   int instruction;
   // Calculate the 32-bit offset relative to the current PC
   unsigned int imm32 = (unsigned int) target - (unsigned int) PC;
   // Split into a 20-bit upper and a 12-bit lower
   unsigned int imm20 = imm32 >> 12;
   unsigned int imm12 = imm32 & 0xfff;
   // If the lower is negative then correct the upper by adding one
   // (the 12-bit immediate is always sign extended, so need to compensate for this)
   if (imm12 & 0x800) {
      imm20 = (imm20 + 1) & 0xfffff;
   }
   // auipc rd, imm20
   instruction = opcodes[AUIPC] | (imm20 << 12) | (rd << RD);
   poke(&instruction, 4);
   return imm12;
}

void assemble (void)
{
   signed char al ;
   signed char *oldesi = esi ;
   int init = 1 ;
   void *oldpc = PC ;
   int rs1_shift;
   int rs2_shift;
   unsigned int flags;

   while (1)
      {
         int mnemonic, instruction = 0 ;

         if (liston & BIT7)
            {
               int tmp ;
               if (liston & BIT6)
                  tmp = stavar[15] ;
               else
                  tmp = stavar[16] ;
               if (tmp >= stavar[12])
                  error (8, NULL) ; // 'Address out of range'
            }

         al = nxt () ;
         esi++ ;

         switch (al)
            {
            case 0:
               esi-- ;
               liston = (liston & 0x0F) | 0x30 ;
               return ;

            case ']':
               liston = (liston & 0x0F) | 0x30 ;
               return ;

            case 0x0D:
               newlin () ;
               if (*esi == 0x0D)
                  break ;
            case ':':
               if (liston & BIT4)
                  {
                     void *p ;
                     int n = PC - oldpc ;
                     if (liston & BIT6)
                        p = OC - n ;
                     else
                        p = PC - n ;

                     do
                        {
                           unsigned int i = *(unsigned int *)p ;
                           sprintf (accs, "%08lX ", (long) (size_t) oldpc) ;
                           switch (n)
                              {
                              case 0: break ;
                              case 1: i &= 0xFF ;
                              case 2: i &= 0xFFFF ;
                              case 3: i &= 0xFFFFFF ;
                              case 4: sprintf (accs + 9, "%0*X ", n*2, i) ;
                                 break ;
                              default: sprintf (accs + 9, "%08X ", i) ;
                              }
                           if (n > 4)
                              {
                                 n -= 4 ;
                                 p += 4 ;
                                 oldpc += 4 ;
                              }
                           else
                              n = 0 ;

                           text (accs) ;

                           if (*oldesi == '.')
                              {
                                 tabit (18) ;
                                 do
                                    {
                                       token (*oldesi++ ) ;
                                    }
                                 while (range0(*oldesi)) ;
                                 while (*oldesi == ' ') oldesi++ ;
                              }
                           tabit (30) ;
                           while ((*oldesi != ':') && (*oldesi != 0x0D))
                              token (*oldesi++) ;
                           crlf () ;
                        }
                     while (n) ;
                  }
               nxt () ;
#ifdef __arm__
               if ((liston & BIT6) == 0)
                  __builtin___clear_cache (oldpc, PC) ;
#endif
               oldpc = PC ;
               oldesi = esi ;
               break ;

            case ';':
               while ((*esi != 0x0D) && (*esi != ':')) esi++ ;
               break ;

            case '.':
               if (init)
                  oldpc = align () ;
               {
                  VAR v ;
                  unsigned char type ;
                  void *ptr = getput (&type) ;
                  if (ptr == NULL)
                     error (16, NULL) ; // 'Syntax error'
                  if (type >= 128)
                     error (6, NULL) ; // 'Type mismatch'
                  if ((liston & BIT5) == 0)
                     {
                        v = loadn (ptr, type) ;
                        if (v.i.n)
                           error (3, NULL) ; // 'Multiple label'
                     }
                  v.i.t = 0 ;
                  v.i.n = (intptr_t) PC ;
                  storen (v, ptr, type) ;
               }
               break ;

            default:
               esi-- ;
               mnemonic = lookup_mnemonic();

               if (mnemonic != OPT)
                  init = 0 ;

               if (mnemonic < 0) {
                     error (16, NULL) ; // 'Syntax error'
                     return; // never reached
               }

               instruction  = opcodes[mnemonic];
               flags        = instruction;
               instruction &= F_CLEAR_MASK;

               // The reversed flag indicates the source operands should be swapper
               if (flags & F_REVERSED) {
                  rs1_shift = RS2;
                  rs2_shift = RS1;
               } else {
                  rs1_shift = RS1;
                  rs2_shift = RS2;
               }


               switch (mnemonic)
                  {
                  case OPT:
                     liston = (liston & 0x0F) | (expri () << 4) ;
                     continue ;

                  case ALIGN:
                     oldpc = align () ;
                     if ((nxt() >= '1') && (*esi <= '9'))
                        {
                           int n = expri () ;
                           if ((n & (n - 1)) || (n & 0xFFFFFF03) || (n == 0))
                              error (16, NULL) ; // 'Syntax error'
                           instruction = 0xE1A00000 ;
                           while (stavar[16] & (n - 1))
                              poke (&instruction, 4) ;
                        }
                     continue ;

                  case DB:
                     {
                        VAR v = expr () ;
                        if (v.s.t == -1)
                           {
                              if (v.s.l > 256)
                                 error (19, NULL) ; // 'String too long'
                              poke (v.s.p + zero, v.s.l) ;
                              continue ;
                           }
                        if (v.i.t)
                           v.i.n = v.f ;
                        poke (&v.i.n, 1) ;
                        continue ;
                     }

                  case DCB:
                  case EQUB:
                     {
                        int n = expri () ;
                        poke (&n, 1) ;
                        continue ; // n.b. not break
                     }

                  case DCW:
                  case EQUW:
                     {
                        int n = expri () ;
                        poke (&n, 2) ;
                        continue ; // n.b. not break
                     }

                  case DCD:
                  case EQUD:
                  case EQUQ:
                     {
                        VAR v = expr () ;
                        long long n ;
                        if (v.s.t == -1)
                           {
                              error (6, NULL) ; // 'Type mismatch'
                           }
                        else if (v.i.t == 0)
                           n = v.i.n ;
                        else
                           n = v.f ;
                        if (mnemonic == EQUQ)
                           {
                              poke (&n, 8) ;
                           }
                        else
                           {
                              poke (&n, 4) ;
                           }
                     }
                     continue ;

                  case DCS:
                  case EQUS:
                     {
                        VAR v = exprs () ;
                        if (v.s.l > 256)
                           error (19, NULL) ; // 'String too long'
                        poke (v.s.p + zero, v.s.l) ;
                        continue ;
                     }

                  case ADD:
                  case SUB:
                  case XOR:
                  case OR:
                  case AND:
                  case SLL:
                  case SRL:
                  case SRA:
                  case SLT:
                  case SLTU:
                  case MUL:
                  case MULH:
                  case MULSU:
                  case MULU:
                  case DIV:
                  case DIVU:
                  case REM:
                  case REMU:
                  case NEG:     // sub  rd, 0, rs
                  case SNEZ:    // sltu rd, zero, rs
                  case SLTZ:    // slt  rd, rs, zero
                  case SGTZ:    // slt  rd, zero, rs
                     {
                        // Format R
                        // e.g. add rd, rs1, rs2
                        instruction |= reg() << RD;
                        comma();
                        instruction |= reg() << rs1_shift;
                        if (!(flags & F_ZERO)) {
                           comma();
                           instruction |= reg() << rs2_shift;
                        }
                     }
                     break;

                  case ADDI:
                  case XORI:
                  case ORI:
                  case ANDI:
                  case SLLI:
                  case SRLI:
                  case SRAI:
                  case SLTI:
                  case SLTUI:
                  case MV:      // addi rd, rs, 0
                  case NOT:     // xori rd, rs, -1
                  case SEQZ:    // sltiu rd, rs, 1
                     {
                        // Format I
                        // e.g. addi rd, rs1, immediate
                        instruction |= reg() << RD;
                        comma();
                        instruction |= reg() << RS1;
                        if (flags & F_IMM12_000) {
                           // Force a imm12 value of 000 (e.g. for mv)
                        } else if (flags & F_IMM12_001) {
                           // Force a imm12 value of 000 (e.g. for mv)
                           instruction |= 0x00100000;
                        } else if (flags & F_IMM12_FFF) {
                           // Force a imm12 value of fff (e.g. for not)
                           instruction |= 0xFFF00000;
                        } else {
                           // Parse an imm12 value
                           comma();
                           instruction |= imm12() << 20;
                        }
                     }
                     break;

                  case LB:
                  case LH:
                  case LW:
                  case LBU:
                  case LHU:
                  case SB:
                  case SH:
                  case SW:
                     {
                        int store = (mnemonic == SB || mnemonic == SH || mnemonic == SW) ? 1 : 0;
                        // Parse the following forms:
                        //     opcode r1, immediate(r2)
                        //     opcode r1, (r2)           ; immediate can be omitted
                        //     opcode r1, symbol         ; this is the load/store global pseudo
                        int r1 = reg();
                        int r2 = 0;
                        unsigned int imm = 0;
                        comma();
                        signed char *old_esi = esi; // save the program pointer
                        // First, try to parse without an immediate
                        int success = 0;
                        if (nxt() == '(') {
                           esi++;
                           if (count_regs() > 0) {
                              r2 = reg();
                              if (nxt() == ')') {
                                 esi++;
                                 success = 1;
                              }
                           }
                        }
                        // If the simple case not found, then backtrack
                        if (!success) {
                           esi = old_esi; // restore the program counter

                           // Parse the immediate or symbol
                           long long value = expri();

                           signed char c = nxt();
                           if (c == '(') {
                              // opcode r1, immediate(r2)
                              esi++;
                              r2 = reg();
                              if (nxt() != ')') {
                                 error (27, NULL) ; // 'Missing )'
                              }
                              esi++;
                              validate_imm12(value);
                              imm = (unsigned int) (value & 0xfff);
                           } else if (store && c == ',') {
                              // store r1, symbol, r2
                              esi++;
                              r2 = reg();
                              imm = generate_auipc(r2, value);
                           } else if (!store && eol(c)) {
                              // load r1, symbol
                              r2 = r1;
                              imm = generate_auipc(r1, value);
                           } else {
                              // something expected
                              error (16, NULL) ; // 'Syntax error'
                           }
                        }

                        // Build the final load/store instruction using r1, imm and r2
                        if (store) {
                           // STORE
                           // Format S
                           // e.g. sb rs2, immediate(rs1)
                           //         ^r1  ^imm      ^r2
                           instruction |= r1 << RS2;
                           instruction |= (imm & 0x01f) << 7;
                           instruction |= (imm & 0xfe0) << 20;
                        } else {
                           // LOAD
                           // Format I
                           // e.g. lb rd, immediate(rs1)
                           //         ^r1 ^imm      ^r2
                           instruction |= r1  << RD;
                           instruction |= imm << 20;
                        }
                        instruction |= r2 << RS1;
                        poke(&instruction, 4);

                     }
                     continue;

                  case BEQ:
                  case BNE:
                  case BLT:
                  case BGE:
                  case BLTU:
                  case BGEU:
                  case BGTU:
                  case BGT:
                  case BLEU:
                  case BLE:
                  case BEQZ:
                  case BNEZ:
                  case BLEZ:
                  case BGEZ:
                  case BLTZ:
                  case BGTZ:
                     {
                        // Format B
                        // bne rs1, rs2, target
                        instruction |= reg() << rs1_shift;
                        comma();
                        // Skip parsing of rs2 in "BxxZ" varients (rs2 will default to zero)
                        if (!(flags & F_ZERO)) {
                           instruction |= reg() << rs2_shift;
                           comma();
                        }
                        int dest = ((void *) (size_t) expri () - PC) >> 1 ;
                        if ((dest < -0x800 || dest >= 0x800) && ((liston & BIT5) != 0)) {
                           error (1, NULL) ; // 'Jump out of range'
                        }
                        unsigned int u = (dest & 0xFFF) << 1;
                        instruction |= ((u >> 12) & 0x01) << 31;
                        instruction |= ((u >>  5) & 0x3f) << 25;
                        instruction |= ((u >>  1) & 0x0f) << 8;
                        instruction |= ((u >> 11) & 0x01) << 7;
                     }
                     break;

                  case J:
                  case JAL:
                     // Format J
                     // jal rd, imm20
                     // jal     imm20     (rd default to ra)
                     // j       imm20     (rd default to 0)
                     {
                        if (count_regs() == 0) {
                           if (mnemonic == JAL) {
                              instruction |= 1 << RD; //  rd = ra
                           }
                        } else {
                           instruction |= reg() << RD;
                           comma();
                        }
                        int dest = ((void *) (size_t) expri () - PC) >> 1 ;
                        if ((dest < -0x80000 || dest >= 0x80000) && ((liston & BIT5) != 0)) {
                           error (1, NULL) ; // 'Jump out of range'
                        }
                        unsigned int u = (dest & 0xFFFFF) << 1;
                        // 20 | 10:1 | 11 : 19:12
                        instruction |= ((u >> 20) & 0x001) << 31;
                        instruction |= ((u >>  1) & 0x3ff) << 21;
                        instruction |= ((u >> 11) & 0x001) << 20;
                        instruction |= ((u >> 12) & 0x0ff) << 12;
                     }
                     break ;

                  case JR:
                  case JALR:
                     // Format I
                     // jalr rd, rs1, imm12
                     // jalr     rs1, imm12   (rd default to ra)
                     // jr       rs1          (rd defaults to zero)
                     // jalr rd, rs1          (imm12 defaults to zero)
                     // jalr     rs1          (imm12 defaults to zero, rd defaults to ra)
                     // jr       rs1          (imm12 defaults to zero, rd defaills to zero)
                     {
                        if (count_regs() == 1) {
                           if (mnemonic == JALR) {
                              instruction |= 1 << RD; //  default rd to ra
                           }
                        } else {
                           instruction |= reg() << RD; // parse rd
                           comma();
                        }
                        instruction |= reg() << RS1; // rs1 is the one mandatory part
                        if (nxt() == ',') {
                           comma();
                           instruction |= imm12() << 20; // parse imm12
                        }
                     }
                     break;

                  case LUI:
                  case AUIPC:
                     // Formal U
                     // e.g. lui rd, immediate
                     // e.g. auipc rd, immediate
                     {
                        instruction |= reg() << RD;
                        comma();
                        instruction |= imm20() << 12;
                     }
                     break;

                  case LA:
                  case CALL:
                  case TAIL:
                     // Pseudo instruction
                     // e.g. la   rd, target
                     //      call rd, target
                     //      tail rd, target
                     //      call     target (rd defaults to ra)
                     //      tail     target (rd defaults to t1)
                     {
                        int rd;
                        if (mnemonic == LA || count_regs() > 0) {
                           rd = reg();
                           comma();
                        } else {
                           rd = (mnemonic == CALL) ? 1 : 6;
                        }
                        // Generate an AUIPC instruction, returning the remaining 12-bit immediate
                        unsigned int imm12 = generate_auipc(rd, expri());
                        if (mnemonic == LA) {
                           // addi rd, rd, imm12
                           // Note: addi could be omitted if imm12=0, but that sometimes
                           // three-pass assembley and causes hard-to-track-down bugs
                           instruction = opcodes[ADDI] | (imm12 << 20) | (rd << RS1) | (rd << RD);
                           poke(&instruction, 4);
                        } else {
                           // jalr rd, rd, imm12
                           instruction = opcodes[JALR] | (imm12 << 20) | (rd << RS1) | (rd << RD);
                           poke(&instruction, 4);
                        }
                     }
                     continue ; // n.b. not break

                  case LI:
                     // Pseudo instruction
                     // e.g. li rd, immediate32
                     {
                        int rd = reg();
                        comma();
                        unsigned int imm32 = (unsigned int) expri ();
                        // Split into a 20-bit upper and a 12-bit lower
                        unsigned int imm20 = imm32 >> 12;
                        unsigned int imm12 = imm32 & 0xfff;
                        // If the lower is negative then correct the upper by adding one
                        // (this is because ADDI always sign extends, so need to compensate for this)
                        if (imm12 & 0x800) {
                           imm20 = (imm20 + 1) & 0xfffff;
                        }
                        // Now compose an optimal sequence of lui and/or addi
                        if (imm20) {
                           // lui rd, imm20
                           instruction = opcodes[LUI] | (imm20 << 12) | (rd << RD);
                           poke(&instruction, 4);
                           // addi rd, rd, imm12
                           // Note: addi could be omitted if imm12=0, but that sometimes
                           // three-pass assembley and causes hard-to-track-down bugs
                           instruction = opcodes[ADDI] | (imm12 << 20) | (rd << RS1) | (rd << RD);
                           poke(&instruction, 4);
                        } else {
                           // addi rd, zero, imm12
                           instruction = opcodes[ADDI] | (imm12 << 20) | (rd << RD);
                           poke(&instruction, 4);
                        }
                     }
                     continue ; // n.b. not break

                  case CSRR:
                  case CSRW:
                  case CSRRC:
                  case CSRRS:
                  case CSRRW:
                     // CSR instructions / pseudo instructions
                     // e.g. csrrw rd, csr, rs
                     //       csrr rd, csr
                     //       csrw     csr, rs
                     {
                        if (mnemonic != CSRW) {
                           instruction |= reg() << RD;
                           comma();
                        }
                        instruction |= csr() << 20;
                        if (mnemonic != CSRR) {
                           comma();
                           instruction |= reg() << RS1;
                        }
                     }
                     break;
                  case CSRWI:
                  case CSRRWI:
                  case CSRRSI:
                  case CSRRCI:
                     // CSR short immediate instrucions / pseudo instructions
                     // e.g. csrrwi rd, csr, imm5
                     //      csrwi      csr, imm5
                     {
                        if (mnemonic != CSRWI) {
                           instruction |= reg() << RD;
                           comma();
                        }
                        instruction |= csr() << 20;
                        comma();
                        instruction |= uimm5() << RS1;
                     }
                     break;

                  default:
                     break;
                  }

               oldpc = align () ;

               poke (&instruction, 4) ;
            }
      }
}

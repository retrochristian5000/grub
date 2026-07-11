/* calc.c -- The calc command.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2026 Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/script_sh.h>


GRUB_MOD_LICENSE ("GPLv3+");


#define CALC_OP_ADD        1
#define CALC_OP_SUB        2
#define CALC_OP_DIV        3
#define CALC_OP_MOD        4
#define CALC_OP_MUL        5
#define CALC_OP_POW        6
#define CALC_OP_AND        7
#define CALC_OP_OR         8
#define CALC_OP_XOR        9
#define CALC_OP_NOT        10
#define CALC_OP_LSHIFT     11
#define CALC_OP_RSHIFT     12


static grub_err_t
grub_cmd_calc(
         grub_extcmd_context_t         ctxt,
         int                           argc,
         char **                       args );


static int
format_result(
         long                          input,
         char *                        dst,
         grub_size_t                   dst_len,
         int                           base,
         grub_size_t                   padding );


static int
parse_number(
         const char *                  arg,
         long *                        nump );


static int
parse_operation(
         const char *                  arg,
         int *                         req_num );


static grub_extcmd_t cmd_calc;


static struct
{
   const char *   name;
   int            code;
   int            req_num;
} calc_ops[] =
{
   { "ADD",    CALC_OP_ADD,     1 },
   { "SUB",    CALC_OP_SUB,     1 },
   { "DIV",    CALC_OP_DIV,     1 },
   { "MOD",    CALC_OP_MOD,     1 },
   { "MUL",    CALC_OP_MUL,     1 },
   { "POW",    CALC_OP_POW,     1 },
   { "AND",    CALC_OP_AND,     1 },
   { "OR",     CALC_OP_OR,      1 },
   { "XOR",    CALC_OP_XOR,     1 },
   { "NOT",    CALC_OP_NOT,     0 },
   { "LSH",    CALC_OP_LSHIFT,  1 },
   { "RSH",    CALC_OP_RSHIFT,  1 },
   { "LSHIFT", CALC_OP_LSHIFT,  1 },
   { "RSHIFT", CALC_OP_RSHIFT,  1 },
   { NULL, 0, 0 }
};


static const struct grub_arg_option options_calc[] =
{
   { "binary",  'B', 0, N_("represent result using bianry."),        0, 0 },
   { "octal",   'O', 0, N_("represent result using octal."),         0, 0 },
   { "decimal", 'D', 0, N_("represent result using decimal."),       0, 0 },
   { "hex",     'H', 0, N_("represent result using hexadecimal."),   0, 0 },
   { "ops",     'l', 0, N_("display list of available operations."), 0, 0 },
   { "set",     's', 0, N_("Set a variable to the result of operation."),
                        N_("VARNAME"), ARG_TYPE_STRING },
   { "pad",     'p', 0, N_("pad result to LENGTH digits with zeros."),
                        N_("LENGTH"), ARG_TYPE_INT },
   { 0, 0, 0, 0, 0, 0 }
};


GRUB_MOD_INIT(calc)
{
   cmd_calc = grub_register_extcmd( "calc", grub_cmd_calc, 0,
                                    N_("NUMBER [EXPRESSION [... [EXPRESSION]]]"),
                                    N_("perform mathematical and bitwise operations."),
                                    options_calc );
}


GRUB_MOD_FINI(calc)
{
   grub_unregister_extcmd (cmd_calc);
}


grub_err_t
grub_cmd_calc(
         grub_extcmd_context_t         ctxt,
         int                           argc,
         char **                       args )
{
   long         num1;
   long         num2;
   long         num3;
   long         count;
   int          code;
   int          pos;
   int          base;
   int          req_num;
   grub_size_t  padding;
   char         res[(sizeof(long)*8) + 5];

   if (ctxt->state[4].set != 0)
   {  grub_printf("Expressions:\n");
      grub_printf("   add NUMBER  - addition\n");
      grub_printf("   sub NUMBER  - subtraction\n");
      grub_printf("   div NUMBER  - integer division\n");
      grub_printf("   mod NUMBER  - modulo\n");
      grub_printf("   mul NUMBER  - multiplication\n");
      grub_printf("   pow NUMBER  - power\n");
      grub_printf("   and NUMBER  - bitwise AND operation\n");
      grub_printf("   or  NUMBER  - bitwise inclusive OR operation\n");
      grub_printf("   xor NUMBER  - bitwise exclusive OR operation\n");
      grub_printf("   not         - bitwise NOT operation\n");
      grub_printf("   lsh NUMBER  - bitwise left shift operation\n");
      grub_printf("   rsh NUMBER  - bitwise right shift operation\n");
      grub_printf("Examples:\n");
      grub_printf("   calc 1 add 3               result:  4\n");
      grub_printf("   calc 7 div 2               result:  3\n");
      grub_printf("   calc 5 mod 2               result:  1\n");
      grub_printf("   calc -- 14 div -3          result: -4\n");
      grub_printf("   calc 907 add 980 mod 1151  result: 736\n");
      return(0);
   };

   padding = 0;
   if (ctxt->state[6].set != 0)
      padding = (grub_size_t)grub_strtoul(ctxt->state[6].arg, NULL, 0);
   if (padding > (sizeof(res)-2))
      return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("padding LENGTH exceeds buffer size")) );

   if (argc < 1)
      return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("missing argument")) );

   if      (ctxt->state[0].set != 0) base =  2;
   else if (ctxt->state[1].set != 0) base =  8;
   else if (ctxt->state[2].set != 0) base = 10;
   else if (ctxt->state[3].set != 0) base = 16;
   else                              base = 10;

   if (parse_number(args[0], &num1) != 0)
      return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("invalid numeric argument")) );

   for(pos = 1; (pos < argc); pos++)
   {  req_num = 0;
      num2    = 0;
      code    = parse_operation(args[pos], &req_num);

      if (req_num != 0)
      {  pos++;
         if (pos >= argc)
            return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("missing argument")) );
         if (parse_number(args[pos], &num2) != 0)
            return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("invalid numeric argument")) );
      };

      switch(code)
      {  case CALC_OP_ADD:    num1 += num2;         break;
         case CALC_OP_SUB:    num1 -= num2;         break;
         case CALC_OP_MUL:    num1 *= num2;         break;
         case CALC_OP_AND:    num1 &= num2;         break;
         case CALC_OP_OR:     num1 |= num2;         break;
         case CALC_OP_XOR:    num1 ^= num2;         break;
         case CALC_OP_NOT:    num1  = ~num1;        break;
         case CALC_OP_LSHIFT: num1  = num1 << num2; break;
         case CALC_OP_RSHIFT: num1  = num1 >> num2; break;

         case CALC_OP_DIV:
              if (num2 == 0)
                 return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("cannot divide by zero")) );
              num1 /= num2;
              break;

         case CALC_OP_MOD:
              if (num2 == 0)
                 return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("cannot divide by zero")) );
              num1 %= num2;
              break;

         case CALC_OP_POW:
              if (num2 == 0)
                 if (num1 == 0)
                    return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("cannot raise zero to the power of zero")) );
              if (num2 < 0)
              {  if (num1 == 0)
                    return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("cannot raise zero to a negative power")) );
                 num1 = 0;
                 break;
              };
              num3 = 1;
              for(count = 0; (count < num2); count++)
                 num3 *= num1;
              num1 = num3;
              break;

         default:
            return( grub_error(GRUB_ERR_BAD_ARGUMENT, N_("unknown operation")) );
      };
   };

   format_result(num1, res, sizeof(res), base, padding);

   if (ctxt->state[5].set == 0)
   {  grub_printf("%s\n", res);
      return(0);
   };

   grub_env_set(ctxt->state[5].arg, res);

   return(0);
}


int
format_result(
         long                          input,
         char *                        dst,
         grub_size_t                   dst_len,
         int                           base,
         grub_size_t                   padding )
{
   grub_size_t    pos;
   long           digit;
   int            neg;
   unsigned long  number;
   grub_size_t    idx;

   pos        = dst_len;
   dst[--pos] = '\0';
   neg        = 0;

   if (input == 0)
      dst[--pos] = '0';
   if (input != 0)
   {  if ( (base == 10) && (input < 0) )
      {   neg    = 1;
          input *= -1;
      };
      number = (unsigned long)input;

      while ( (number != 0) && (pos > 0) )
      {   digit  = number % (unsigned)base;
          number = number / (unsigned)base;
          if (digit > 9)
             dst[--pos] = (long)'a' + (digit-10);
          else
             dst[--pos] = (long)'0' + digit;
      };

      if (number != 0)
         return(-1);
   };

   padding = (padding > (dst_len-3))
           ? dst_len - 3
           : padding;
   while((dst_len-pos) <= padding)
      dst[--pos] = '0';

   switch(base)
   {  case 2:
         if (pos < 2)
            return(-1);
         dst[--pos] = 'b';
         dst[--pos] = '0';
         break;

      case 8:
         if (pos < 1)
            return(-1);
         dst[--pos] = '0';
         break;

      case 10:
         if (neg != 0)
         {  if (pos < 1)
               return(-1);
            dst[--pos] = '-';
         };
         break;

      case 16:
         if (pos < 2)
            return(-1);
         dst[--pos] = 'x';
         dst[--pos] = '0';
         break;

      default:
         return(-1);
   };

   if (pos != 0)
   {  for(idx = 0; ((idx+pos) < dst_len); idx++)
         dst[idx] = dst[idx+pos];
      dst[idx] = '\0';
   };

   return(0);
}


int
parse_number(
         const char *                  arg,
         long *                        nump )
{
   int            base;
   long           num;
   const char *   numstr;
   const char *   ptr;

   if (arg[0] == '0')
   {  switch(arg[1])
      {  case 'b':
            base   = 2;
            numstr = &arg[2];
            break;

         case 'x':
            base   = 16;
            numstr = &arg[2];
            break;

         case '\0':
            *nump = 0;
            return(0);

         default:
            if ( (arg[1] < '0') || (arg[1] > '7'))
               return(-1);
            base   = 8;
            numstr = &arg[1];
            break;
      };
   } else
   {  base   = 10;
      numstr = arg;
   };

   grub_errno = GRUB_ERR_NONE;
   num = grub_strtol(numstr, &ptr, base);
   if (grub_errno != GRUB_ERR_NONE)
      return(-1);
   if  ( (ptr == NULL) || (ptr[0] != '\0') )
      return(-1);

   *nump = num;

   return(0);
}


int
parse_operation(
         const char *                  arg,
         int *                         req_num )
{
   int idx;

   if (arg == NULL)
      return(-1);

   if (req_num != NULL)
      *req_num = 0;

   for(idx = 0; (calc_ops[idx].name != NULL); idx++)
   {  if (grub_strcasecmp(calc_ops[idx].name, arg) == 0)
      {  if (req_num != NULL)
            *req_num = calc_ops[idx].req_num;
         return(calc_ops[idx].code);
      };
   };

   return(-1);
}

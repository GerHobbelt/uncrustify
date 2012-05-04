/*
 * Generated by make_token_names.sh on Fri May  4 19:00:49 CEST 2012
 */
#ifndef TOKEN_NAMES_H_INCLUDED
#define TOKEN_NAMES_H_INCLUDED

const char *token_names[] =
{
   "NONE",
   "SOF",
   "EOF",
   "UNKNOWN",
   "JUNK",
   "WHITESPACE",
   "SPACE",
   "NEWLINE",
   "NL_CONT",
   "COMMENT_CPP",
   "COMMENT",
   "COMMENT_MULTI",
   "COMMENT_EMBED",
   "COMMENT_START",
   "COMMENT_END",
   "COMMENT_WHOLE",
   "COMMENT_ENDIF",
   "IGNORED",
   "WORD",
   "NUMBER",
   "NUMBER_FP",
   "STRING",
   "STRING_MULTI",
   "IF",
   "ELSE",
   "ELSEIF",
   "FOR",
   "WHILE",
   "WHILE_OF_DO",
   "SWITCH",
   "CASE",
   "DO",
   "VOLATILE",
   "TYPEDEF",
   "STRUCT",
   "ENUM",
   "SIZEOF",
   "RETURN",
   "BREAK",
   "UNION",
   "GOTO",
   "CONTINUE",
   "C_CAST",
   "CPP_CAST",
   "D_CAST",
   "TYPE_CAST",
   "TYPENAME",
   "TEMPLATE",
   "ASSIGN",
   "ASSIGN_NL",
   "SASSIGN",
   "COMPARE",
   "SCOMPARE",
   "BOOL",
   "SBOOL",
   "ARITH",
   "SARITH",
   "DEREF",
   "INCDEC_BEFORE",
   "INCDEC_AFTER",
   "MEMBER",
   "DC_MEMBER",
   "C99_MEMBER",
   "INV",
   "DESTRUCTOR",
   "NOT",
   "D_TEMPLATE",
   "ADDR",
   "NEG",
   "POS",
   "STAR",
   "PLUS",
   "MINUS",
   "AMP",
   "BYREF",
   "POUND",
   "PREPROC",
   "PREPROC_INDENT",
   "PREPROC_BODY",
   "PP",
   "ELLIPSIS",
   "RANGE",
   "SEMICOLON",
   "VSEMICOLON",
   "COLON",
   "CASE_COLON",
   "CLASS_COLON",
   "D_ARRAY_COLON",
   "COND_COLON",
   "QUESTION",
   "COMMA",
   "ASM",
   "ATTRIBUTE",
   "CATCH",
   "CLASS",
   "DELETE",
   "EXPORT",
   "FRIEND",
   "NAMESPACE",
   "NEW",
   "OPERATOR",
   "OPERATOR_VAL",
   "PRIVATE",
   "PRIVATE_COLON",
   "THROW",
   "TRY",
   "USING",
   "USING_STMT",
   "D_WITH",
   "D_MODULE",
   "SUPER",
   "DELEGATE",
   "BODY",
   "DEBUG",
   "DEBUGGER",
   "INVARIANT",
   "UNITTEST",
   "UNSAFE",
   "FINALLY",
   "IMPORT",
   "D_SCOPE",
   "D_SCOPE_IF",
   "LAZY",
   "D_MACRO",
   "D_VERSION",
   "D_VERSION_IF",
   "PAREN_OPEN",
   "PAREN_CLOSE",
   "ANGLE_OPEN",
   "ANGLE_CLOSE",
   "SPAREN_OPEN",
   "SPAREN_CLOSE",
   "FPAREN_OPEN",
   "FPAREN_CLOSE",
   "BRACE_OPEN",
   "BRACE_CLOSE",
   "VBRACE_OPEN",
   "VBRACE_CLOSE",
   "SQUARE_OPEN",
   "SQUARE_CLOSE",
   "TSQUARE",
   "MACRO_OPEN",
   "MACRO_CLOSE",
   "MACRO_ELSE",
   "LABEL",
   "LABEL_COLON",
   "FUNCTION",
   "FUNC_CALL",
   "FUNC_CALL_USER",
   "FUNC_DEF",
   "FUNC_PROTO",
   "FUNC_CLASS",
   "FUNC_CTOR_VAR",
   "FUNC_WRAP",
   "PROTO_WRAP",
   "MACRO_FUNC",
   "MACRO",
   "QUALIFIER",
   "EXTERN",
   "ALIGN",
   "TYPE",
   "PTR_TYPE",
   "TYPE_WRAP",
   "CPP_LAMBDA",
   "CPP_LAMBDA_RET",
   "BIT_COLON",
   "BASED",
   "DECLSPEC",
   "EXCEPT",
   "IF_EXISTS",
   "LEAVE",
   "INHERITANCE",
   "UNALIGNED",
   "OC_DYNAMIC",
   "OC_END",
   "OC_IMPL",
   "OC_INTF",
   "OC_PROTOCOL",
   "OC_PROTO_LIST",
   "OC_PROPERTY",
   "OC_CLASS",
   "OC_CLASS_EXT",
   "OC_CATEGORY",
   "OC_SCOPE",
   "OC_MSG",
   "OC_MSG_CLASS",
   "OC_MSG_FUNC",
   "OC_MSG_NAME",
   "OC_MSG_SPEC",
   "OC_MSG_DECL",
   "OC_RTYPE",
   "OC_COLON",
   "OC_SEL",
   "OC_SEL_NAME",
   "OC_BLOCK_ARG",
   "OC_BLOCK_TYPE",
   "OC_BLOCK_EXPR",
   "OC_BLOCK_CARET",
   "PP_DEFINE",
   "PP_DEFINED",
   "PP_INCLUDE",
   "PP_IF",
   "PP_ELSE",
   "PP_ENDIF",
   "PP_ASSERT",
   "PP_EMIT",
   "PP_ENDINPUT",
   "PP_ERROR",
   "PP_FILE",
   "PP_LINE",
   "PP_SECTION",
   "PP_UNDEF",
   "PP_BODYCHUNK",
   "PP_PRAGMA",
   "PP_REGION",
   "PP_ENDREGION",
   "PP_REGION_INDENT",
   "PP_IF_INDENT",
   "PP_OTHER",
   "CHAR",
   "DEFINED",
   "FORWARD",
   "NATIVE",
   "STATE",
   "STOCK",
   "TAGOF",
   "DOT",
   "TAG",
   "TAG_COLON",
   "LOCK",
   "AS",
   "IN",
   "BRACED",
   "THIS",
   "BASE",
   "DEFAULT",
   "GETSET",
   "GETSET_EMPTY",
   "CONCAT",
   "CS_SQ_STMT",
   "CS_SQ_COLON",
   "CS_PROPERTY",
   "CS_ARRAY",
   "AS_FRIEND",
   "GCNEW",
   "GENERIC",
   "WHERE",
   "INITONLY",
   "INTERIOR_PTR",
   "LITERAL",
   "NULLPTR",
   "BOX",
   "GC",
   "IDENTIFIER",
   "NOGC",
   "PIN",
   "PROPERTY",
   "SEALED",
   "VALUE",
   "EVENT",
   "HOOK",
   "INTERFACE",
   "RAISE",
   "UNHOOK",
   "UUIDOF",
   "SQL_EXEC",
   "SQL_BEGIN",
   "SQL_END",
   "SQL_WORD",
   "CONSTRUCT",
   "LAMBDA",
   "ASSERT",
   "CONCATENATE",
   "XML_OPSTART",
   "XML_OPEND",
   "PHP_ARRAY",
   "CFUNCTION",
   "CLONE",
   "DECLARE",
   "ENDDECLARE",
   "ENDFOR",
   "ENDFOREACH",
   "ENDIF",
   "ENDSWITCH",
   "ENDWHILE",
   "GLOBAL",
   "INCLUDE",
   "INCLUDE_ONCE",
   "OLD_FUNCTION",
   "REQUIRE",
   "REQUIRE_ONCE",
   "USE",
};

#endif /* TOKEN_NAMES_H_INCLUDED */


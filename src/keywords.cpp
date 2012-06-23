/**
 * @file keywords.cpp
 * Manages the table of keywords.
 *
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "uncrustify_types.h"
#include "prototypes.h"
#include "char_table.h"
#include "args.h"
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <map>
#include "unc_ctype.h"

using namespace std;

/* Dynamic keyword map */
typedef map<string, c_token_t> dkwmap;
static dkwmap dkwm;


/**
 * interesting static keywords - keep sorted.
 * Table should include the Name, Type, and Language flags.
 */
static const chunk_tag_t keywords[] =
{
   //{ "each",             CT_EACH,         LANG_CPP | LANG_CS                                                          },
   { "$this",            CT_THIS,         LANG_PHP                                                                    },
   { "@catch",           CT_CATCH,        LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@dynamic",         CT_OC_DYNAMIC,   LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@end",             CT_OC_END,       LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@finally",         CT_TRY,          LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@implementation",  CT_OC_IMPL,      LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@interface",       CT_OC_INTF,      LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@private",         CT_PRIVATE,      LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@property",        CT_OC_PROPERTY,  LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@protocol",        CT_OC_PROTOCOL,  LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@selector",        CT_OC_SEL,       LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@synthesize",      CT_OC_DYNAMIC,   LANG_OC | LANG_CPP | LANG_C                                                 },
   { "@try",             CT_TRY,          LANG_OC | LANG_CPP | LANG_C                                                 },
   { "_Bool",            CT_TYPE,         LANG_CPP                                                                    },
   { "_Complex",         CT_TYPE,         LANG_CPP                                                                    },
   { "_Imaginary",       CT_TYPE,         LANG_CPP                                                                    },
   { "__CLASS__",        CT_WORD,         LANG_PHP																	  },
   { "__DIR__",			 CT_WORD,         LANG_PHP																	  },
   { "__FILE__",         CT_WORD,         LANG_PHP																	  },
   { "__FUNCTION__",     CT_WORD,         LANG_PHP																	  },
   { "__LINE__",         CT_WORD,         LANG_PHP																	  },
   { "__METHOD__",       CT_WORD,         LANG_PHP																	  },
   { "__NAMESPACE__",    CT_WORD,         LANG_PHP																	  },
   { "__abstract",		 CT_QUALIFIER,    LANG_CPPNET | LANG_CS														  },
   { "__alignof",		 CT_SIZEOF,       LANG_C | LANG_CPP | LANG_CS			                                      }, // PAWN
   { "__asm",			 CT_ASM,          LANG_C | LANG_CPP															  },
   { "__assume",         CT_FUNCTION,     LANG_C | LANG_CPP															  },
   { "__attribute__",    CT_ATTRIBUTE,    LANG_C | LANG_CPP                                                           },
   { "__based",          CT_BASED,        LANG_C | LANG_CPP															  },
   { "__box",			 CT_BOX,		  LANG_CPPNET																  },
   { "__cdecl",			 CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__const__",        CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__declspec",		 CT_DECLSPEC,     LANG_C | LANG_CPP															  },
   { "__delegate",       CT_DELEGATE,     LANG_CPPNET																  },
   { "__event",          CT_EVENT,        LANG_CPP | LANG_CPPNET													  },
   { "__except",         CT_EXCEPT,       LANG_C | LANG_CPP															  },
   { "__fastcall",       CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__finally",        CT_FINALLY,      LANG_C | LANG_CPP															  },
   { "__forceinline",    CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__gc",			 CT_GC,			  LANG_CPPNET																  },
   { "__hook",			 CT_HOOK,         LANG_CPP | LANG_CPPNET													  },
   { "__identifier",	 CT_IDENTIFIER,	  LANG_CPPNET																  },
   { "__if_exists",      CT_IF_EXISTS,    LANG_CPP																	  },
   { "__if_not_exists",  CT_IF_EXISTS,    LANG_CPP																	  },
   { "__inline",         CT_QUALIFIER,    LANG_C | LANG_CPP                                                           }, /* MSVC */
   { "__inline__",       CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__int16",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__int32",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__int64",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__int8",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__interface",		 CT_INTERFACE,    LANG_CPP | LANG_CPPNET													  },
   { "__leave",			 CT_LEAVE,        LANG_C | LANG_CPP															  },
   { "__m128",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__m128d",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__m128i",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__m64",			 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__multiple_inheritance", CT_INHERITANCE, LANG_CPP																  },
   { "__nogc",           CT_NOGC,		  LANG_CPPNET																  },
   { "__noop",           CT_FUNCTION,     LANG_C | LANG_CPP                                                           },
   { "__pin",			 CT_PIN,	      LANG_CPPNET																  },
   { "__property",       CT_PROPERTY,	  LANG_CPPNET																  },
   { "__raise",          CT_RAISE,		  LANG_CPP | LANG_CPPNET													  },
   { "__restrict",       CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__sealed",         CT_SEALED,       LANG_CPPNET																  },
   { "__signed__",       CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "__single_inheritance", CT_INHERITANCE, LANG_CPP																  },
   { "__stdcall",        CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__super",			 CT_SUPER,		  LANG_CPP																	  },
   { "__traits",         CT_QUALIFIER,    LANG_D                                                                      },
   { "__try",            CT_TRY,          LANG_C | LANG_CPP															  },
   { "__try_cast",       CT_TYPE_CAST,    LANG_CPPNET																  },
   { "__typeof__",       CT_SIZEOF,       LANG_C | LANG_CPP                                                           },
   { "__unaligned",      CT_UNALIGNED,    LANG_C | LANG_CPP                                                           },
   { "__unhook",		 CT_UNHOOK,       LANG_CPP | LANG_CPPNET													  },
   { "__uuidof",		 CT_UUIDOF,       LANG_CPP | LANG_CPPNET													  },
   { "__value",		     CT_VALUE,        LANG_CPPNET																  },
   { "__virtual_inheritance", CT_INHERITANCE, LANG_CPP																  },
   { "__volatile__",     CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__w64",			 CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "__wchar_t",		 CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "abstract",         CT_QUALIFIER,    LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA | LANG_PHP             },
   { "add",              CT_GETSET,       LANG_CS                                                                     },
   { "alias",            CT_QUALIFIER,    LANG_D                                                                      },
   { "align",            CT_ALIGN,        LANG_D                                                                      },
   { "alignof",          CT_SIZEOF,       LANG_C | LANG_CPP                                                           },
   { "and",              CT_SBOOL,        LANG_C | LANG_CPP | FLAG_PP			                                      },
   { "and",              CT_SBOOL,        LANG_PHP																	  },
   { "and_eq",           CT_SASSIGN,      LANG_C | LANG_CPP                                                           },
   { "array",            CT_CS_ARRAY,     LANG_CS																	  },
   { "array",            CT_PHP_ARRAY,    LANG_PHP																	  },
   { "as",               CT_AS,           LANG_CS | LANG_PHP                                                          },
   { "as_friend",        CT_AS_FRIEND,    LANG_CS																	  },
   { "asm",              CT_ASM,          LANG_C | LANG_CPP | LANG_D                                                  },
   { "assert",           CT_ASSERT,       LANG_JAVA                                                                   },
   { "assert",           CT_FUNCTION,     LANG_D | LANG_PAWN                                                          }, // PAWN
   { "assert",           CT_PP_ASSERT,    LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "auto",             CT_QUALIFIER,    LANG_C | LANG_CPP | LANG_D                                                  },
   { "base",             CT_BASE,         LANG_CS | LANG_VALA                                                         },
   { "bit",              CT_TYPE,         LANG_D                                                                      },
   { "bitand",           CT_ARITH,        LANG_C | LANG_CPP                                                           },
   { "bitor",            CT_ARITH,        LANG_C | LANG_CPP                                                           },
   { "body",             CT_BODY,         LANG_D                                                                      },
   { "bool",             CT_TYPE,         LANG_CPP | LANG_CS | LANG_VALA                                              },
   { "boolean",          CT_TYPE,         LANG_JAVA | LANG_ECMA                                                       },
   { "break",            CT_BREAK,        LANG_ALL                                                                    }, // PAWN
   { "byte",             CT_TYPE,         LANG_CS | LANG_D | LANG_JAVA | LANG_ECMA                                    },
   { "callback",         CT_QUALIFIER,    LANG_VALA                                                                   },
   { "case",             CT_CASE,         LANG_ALL                                                                    }, // PAWN
   { "cast",             CT_D_CAST,       LANG_D                                                                      },
   { "catch",            CT_CATCH,        LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_ECMA | LANG_PHP              },
   { "cdecl",            CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "cdouble",          CT_TYPE,         LANG_D                                                                      },
   { "cent",             CT_TYPE,         LANG_D                                                                      },
   { "cfloat",           CT_TYPE,         LANG_D                                                                      },
   { "cfunction",        CT_CFUNCTION,    LANG_PHP																	  },
   { "char",             CT_CHAR,         LANG_PAWN                                                                   }, // PAWN
   { "char",             CT_TYPE,         LANG_ALLC                                                                   },
   { "checked",          CT_QUALIFIER,    LANG_CS                                                                     },
   { "class",            CT_CLASS,        LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA | LANG_PHP  },
   { "clone",            CT_CLONE,        LANG_PHP																	  },
   { "compl",            CT_ARITH,        LANG_C | LANG_CPP                                                           },
   { "const",            CT_QUALIFIER,    LANG_ALL                                                                    }, // PAWN
   { "const_cast",       CT_TYPE_CAST,    LANG_CPP                                                                    },
   { "constexpr",        CT_QUALIFIER,    LANG_CPP                                                                    },
   { "construct",        CT_CONSTRUCT,    LANG_VALA                                                                   },
   { "continue",         CT_CONTINUE,     LANG_ALL                                                                    }, // PAWN
   { "creal",            CT_TYPE,         LANG_D                                                                      },
   { "dchar",            CT_TYPE,         LANG_D                                                                      },
   { "debug",            CT_DEBUG,        LANG_D                                                                      },
   { "debugger",         CT_DEBUGGER,     LANG_ECMA                                                                   },
   { "declare",          CT_DECLARE,      LANG_PHP																	  },
   { "default",          CT_DEFAULT,      LANG_ALL                                                                    }, // PAWN
   { "define",           CT_PP_DEFINE,    LANG_ALLC | FLAG_PP                                                         }, // PAWN
   { "defined",          CT_DEFINED,      LANG_PAWN | LANG_PHP                                                        }, // PAWN, PHP
   { "defined",          CT_PP_DEFINED,   LANG_ALLC | FLAG_PP                                                         },
   { "delegate",         CT_DELEGATE,     LANG_CS | LANG_D                                                            },
   { "delete",           CT_DELETE,       LANG_CPP | LANG_D | LANG_ECMA                                               },
   { "deprecated",       CT_QUALIFIER,    LANG_D                                                                      },
   { "die",				 CT_FUNCTION,     LANG_PHP																	  },
   { "do",               CT_DO,           LANG_ALL                                                                    }, // PAWN
   { "double",           CT_TYPE,         LANG_ALLC                                                                   },
   { "dynamic_cast",     CT_TYPE_CAST,    LANG_CPP                                                                    },
   { "echo",			 CT_FUNCTION,     LANG_PHP																	  },
   { "elif",             CT_PP_ELSE,      LANG_ALLC | FLAG_PP                                                         },
   { "else",             CT_ELSE,         LANG_ALL                                                                    }, // PAWN
   { "else",             CT_PP_ELSE,      LANG_ALLC | FLAG_PP                                                         }, // PAWN
   { "elseif",           CT_ELSE,         LANG_PHP																	  },
   { "elseif",           CT_PP_ELSE,      LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "emit",             CT_PP_EMIT,      LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "empty",			 CT_FUNCTION,	  LANG_PHP																	  },
   { "enddeclare",		 CT_ENDDECLARE,	  LANG_PHP																	  },
   { "endfor",			 CT_ENDFOR,		  LANG_PHP																	  },
   { "endforeach",		 CT_ENDFOREACH,	  LANG_PHP																	  },
   { "endif",			 CT_ENDIF,		  LANG_PHP																	  },
   { "endif",            CT_PP_ENDIF,     LANG_ALL | FLAG_PP                                                          }, // PAWN
   { "endinput",         CT_PP_ENDINPUT,  LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "endregion",        CT_PP_ENDREGION, LANG_ALL | FLAG_PP                                                          },
   { "endscript",        CT_PP_ENDINPUT,  LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "endswitch",		 CT_ENDSWITCH,	  LANG_PHP																	  },
   { "endwhile",		 CT_ENDWHILE,	  LANG_PHP																	  },
   { "enum",             CT_ENUM,         LANG_ALL                                                                    }, // PAWN
   { "error",            CT_PP_ERROR,     LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "eval",			 CT_FUNCTION,	  LANG_PHP																	  },
   { "event",            CT_TYPE,         LANG_CS                                                                     },
   { "exit",             CT_FUNCTION,     LANG_PAWN | LANG_PHP                                                        }, // PAWN
   { "explicit",         CT_TYPE,         LANG_C | LANG_CPP | LANG_CS                                                 },
   { "export",           CT_EXPORT,       LANG_C | LANG_CPP | LANG_D | LANG_ECMA                                      },
   { "extends",          CT_QUALIFIER,    LANG_JAVA | LANG_ECMA | LANG_PHP                                            },
   { "extern",           CT_EXTERN,       LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_VALA                            },
   { "false",            CT_WORD,         LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA                         },
   { "file",             CT_PP_FILE,      LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "final",            CT_QUALIFIER,    LANG_D | LANG_ECMA | LANG_PHP                                               },
   { "finally",          CT_FINALLY,      LANG_D | LANG_CS | LANG_ECMA                                                },
   { "flags",            CT_TYPE,         LANG_VALA                                                                   },
   { "float",            CT_TYPE,         LANG_ALLC                                                                   },
   { "for",              CT_FOR,          LANG_ALL                                                                    }, // PAWN
   { "foreach",          CT_FOR,          LANG_CS | LANG_D | LANG_VALA | LANG_PHP                                     },
   { "foreach_reverse",  CT_FOR,          LANG_D                                                                      },
   { "forward",          CT_FORWARD,      LANG_PAWN                                                                   }, // PAWN
   { "friend",           CT_FRIEND,       LANG_CPP                                                                    },
   { "function",         CT_FUNCTION,     LANG_D | LANG_ECMA | LANG_PHP                                               },
   { "gcnew",            CT_GCNEW,        LANG_CPPNET																  },
   { "generic",          CT_GENERIC,      LANG_CPPNET | LANG_CS														  },
   { "get",              CT_GETSET,       LANG_CS | LANG_VALA                                                         },
   { "global",			 CT_GLOBAL,	      LANG_PHP																	  },
   { "goto",             CT_GOTO,         LANG_ALL                                                                    }, // PAWN
   { "idouble",          CT_TYPE,         LANG_D                                                                      },
   { "if",               CT_IF,           LANG_ALL                                                                    }, // PAWN
   { "if",               CT_PP_IF,        LANG_ALL | FLAG_PP                                                          }, // PAWN
   { "ifdef",            CT_PP_IF,        LANG_ALLC | FLAG_PP                                                         },
   { "ifloat",           CT_TYPE,         LANG_D                                                                      },
   { "ifndef",           CT_PP_IF,        LANG_ALLC | FLAG_PP                                                         },
   { "implements",       CT_QUALIFIER,    LANG_JAVA | LANG_ECMA | LANG_PHP                                            },
   { "implicit",         CT_QUALIFIER,    LANG_CS                                                                     },
   { "import",           CT_IMPORT,       LANG_D | LANG_JAVA | LANG_ECMA                                              }, // fudged to get indenting
   { "import",           CT_PP_INCLUDE,   LANG_OC | FLAG_PP                                                           }, // ObjectiveC version of include
   { "in",               CT_IN,           LANG_D | LANG_CPP | LANG_CS | LANG_VALA | LANG_ECMA                         },
   { "include",			 CT_INCLUDE,      LANG_PHP																	  },
   { "include",          CT_PP_INCLUDE,   LANG_C | LANG_CPP | LANG_PAWN | FLAG_PP                                     }, // PAWN
   { "include_once",     CT_INCLUDE_ONCE, LANG_PHP																	  },
   { "initonly",         CT_INITONLY,     LANG_CPPNET        														  },
   { "inline",           CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "inout",            CT_QUALIFIER,    LANG_D                                                                      },
   { "instanceof",       CT_SIZEOF,       LANG_JAVA | LANG_ECMA | LANG_PHP                                            },
   { "int",              CT_TYPE,         LANG_ALLC                                                                   },
   { "interface",        CT_CLASS,        LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA | LANG_PHP },
   { "interior_ptr",     CT_INTERIOR_PTR, LANG_CPPNET																  },
   { "internal",         CT_QUALIFIER,    LANG_CS                                                                     },
   { "invariant",        CT_INVARIANT,    LANG_D                                                                      },
   { "ireal",            CT_TYPE,         LANG_D                                                                      },
   { "is",               CT_SCOMPARE,     LANG_D | LANG_CS | LANG_VALA                                                },
   { "isset",			 CT_FUNCTION,	  LANG_PHP																	  },
   { "lazy",             CT_LAZY,         LANG_D                                                                      },
   { "line",             CT_PP_LINE,      LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "list",			 CT_FUNCTION,     LANG_PHP																	  },
   { "literal",          CT_LITERAL,      LANG_CPPNET																  },
   { "lock",             CT_LOCK,         LANG_CS | LANG_VALA                                                         },
   { "long",             CT_TYPE,         LANG_ALLC                                                                   },
   { "macro",            CT_D_MACRO,      LANG_D                                                                      },
   { "mixin",            CT_CLASS,        LANG_D                                                                      }, // may need special handling
   { "module",           CT_D_MODULE,     LANG_D                                                                      },
   { "mutable",          CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "namespace",        CT_NAMESPACE,    LANG_C | LANG_CPP | LANG_CS | LANG_VALA | LANG_PHP                          },
   { "native",           CT_NATIVE,       LANG_PAWN                                                                   }, // PAWN
   { "native",           CT_QUALIFIER,    LANG_JAVA | LANG_ECMA                                                       },
   { "new",              CT_NEW,          LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_PAWN | LANG_VALA | LANG_ECMA | LANG_PHP }, // PAWN
   { "not",              CT_SARITH,       LANG_C | LANG_CPP                                                           },
   { "not_eq",           CT_SCOMPARE,     LANG_C | LANG_CPP                                                           },
   { "null",             CT_TYPE,         LANG_CS | LANG_D | LANG_JAVA | LANG_VALA                                    },
   { "nullptr",          CT_NULLPTR,      LANG_CPPNET																  },
   { "object",           CT_TYPE,         LANG_CS                                                                     },
   { "old_function",     CT_OLD_FUNCTION, LANG_PHP																	  },
   { "operator",         CT_OPERATOR,     LANG_CPP | LANG_CS | LANG_PAWN                                              }, // PAWN
   { "or",               CT_SBOOL,        LANG_C | LANG_CPP | FLAG_PP                                                 },
   { "or",               CT_SBOOL,        LANG_PHP																	  },
   { "or_eq",            CT_SASSIGN,      LANG_C | LANG_CPP                                                           },
   { "out",              CT_QUALIFIER,    LANG_CS | LANG_D | LANG_VALA                                                },
   { "override",         CT_QUALIFIER,    LANG_CS | LANG_D | LANG_VALA                                                },
   { "package",          CT_NAMESPACE,    LANG_D | LANG_JAVA | LANG_ECMA                                              },
   { "params",           CT_TYPE,         LANG_CS                                                                     },
   { "pragma",           CT_PP_PRAGMA,    LANG_ALL | FLAG_PP                                                          }, // PAWN
   { "print",			 CT_FUNCTION,	  LANG_PHP																	  },
   { "private",          CT_PRIVATE,      LANG_ALLC | LANG_PHP                                                        }, // not C
   { "protected",        CT_PRIVATE,      LANG_ALLC | LANG_PHP                                                        }, // not C
   { "public",           CT_PRIVATE,      LANG_ALL & ~LANG_C                                                          }, // PAWN // not C
   { "readonly",         CT_QUALIFIER,    LANG_CS                                                                     },
   { "real",             CT_TYPE,         LANG_D                                                                      },
   { "ref",              CT_QUALIFIER,    LANG_CPPNET | LANG_CS | LANG_VALA                                           },
   { "region",           CT_PP_REGION,    LANG_ALL | FLAG_PP                                                          },
   { "register",         CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "reinterpret_cast", CT_TYPE_CAST,    LANG_C | LANG_CPP                                                           },
   { "remove",           CT_GETSET,       LANG_CS                                                                     },
   { "require",          CT_REQUIRE,      LANG_PHP																	  },
   { "require_once",     CT_REQUIRE_ONCE, LANG_PHP																	  },
   { "restrict",         CT_QUALIFIER,    LANG_C | LANG_CPP                                                           },
   { "return",           CT_RETURN,       LANG_ALL                                                                    }, // PAWN
   { "safe_cast",        CT_TYPE_CAST,    LANG_CPPNET																  },
   { "sbyte",            CT_TYPE,         LANG_CS                                                                     },
   { "scope",            CT_D_SCOPE,      LANG_D                                                                      },
   { "sealed",           CT_QUALIFIER,    LANG_CS                                                                     },
   { "section",          CT_PP_SECTION,   LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "set",              CT_GETSET,       LANG_CS | LANG_VALA                                                         },
   { "short",            CT_TYPE,         LANG_ALLC                                                                   },
   { "signal",           CT_PRIVATE,      LANG_VALA                                                                   },
   { "signals",          CT_PRIVATE,      LANG_CPP                                                                    },
   { "signed",           CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "sizeof",           CT_SIZEOF,       LANG_C | LANG_CPP | LANG_CS | LANG_PAWN                                     }, // PAWN
   { "sleep",            CT_FUNCTION,     LANG_C | LANG_CPP | LANG_CS | LANG_PAWN                                     }, // PAWN
   { "stackalloc",       CT_NEW,          LANG_CS                                                                     },
   { "state",            CT_STATE,        LANG_PAWN                                                                   }, // PAWN
   { "static",           CT_QUALIFIER,    LANG_ALL                                                                    }, // PAWN
   { "static_cast",      CT_TYPE_CAST,    LANG_CPP                                                                    },
   { "stock",            CT_STOCK,        LANG_PAWN                                                                   }, // PAWN
   { "strictfp",         CT_QUALIFIER,    LANG_JAVA                                                                   },
   { "string",           CT_TYPE,         LANG_CS                                                                     },
   { "struct",           CT_STRUCT,       LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_VALA                            },
   { "super",            CT_SUPER,        LANG_D | LANG_JAVA | LANG_ECMA                                              },
   { "switch",           CT_SWITCH,       LANG_ALL                                                                    }, // PAWN
   { "synchronized",     CT_QUALIFIER,    LANG_D | LANG_JAVA | LANG_ECMA                                              },
   { "tagof",            CT_TAGOF,        LANG_PAWN                                                                   }, // PAWN
   { "template",         CT_TEMPLATE,     LANG_CPP | LANG_D                                                           },
   { "this",             CT_THIS,         LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA | LANG_ECMA             },
   { "throw",            CT_THROW,        LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_ECMA | LANG_PHP              },
   { "throws",           CT_QUALIFIER,    LANG_JAVA | LANG_ECMA                                                       },
   { "transient",        CT_QUALIFIER,    LANG_JAVA | LANG_ECMA                                                       },
   { "true",             CT_WORD,         LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_VALA                         },
   { "try",              CT_TRY,          LANG_CPP | LANG_CS | LANG_D | LANG_JAVA | LANG_ECMA | LANG_PHP              },
   { "tryinclude",       CT_PP_INCLUDE,   LANG_PAWN | FLAG_PP                                                         }, // PAWN
   { "typedef",          CT_TYPEDEF,      LANG_C | LANG_CPP | LANG_D                                                  },
   { "typeid",           CT_SIZEOF,       LANG_C | LANG_CPP | LANG_D                                                  },
   { "typename",         CT_TYPENAME,     LANG_CPP                                                                    },
   { "typeof",           CT_SIZEOF,       LANG_C | LANG_CPP | LANG_CS | LANG_D | LANG_VALA | LANG_ECMA                },
   { "ubyte",            CT_TYPE,         LANG_D                                                                      },
   { "ucent",            CT_TYPE,         LANG_D                                                                      },
   { "uint",             CT_TYPE,         LANG_CS | LANG_D                                                            },
   { "ulong",            CT_TYPE,         LANG_CS | LANG_D                                                            },
   { "unchecked",        CT_QUALIFIER,    LANG_CS                                                                     },
   { "undef",            CT_PP_UNDEF,     LANG_ALL | FLAG_PP                                                          }, // PAWN
   { "union",            CT_UNION,        LANG_C | LANG_CPP | LANG_D                                                  },
   { "unittest",         CT_UNITTEST,     LANG_D                                                                      },
   { "unsafe",           CT_UNSAFE,       LANG_CS                                                                     },
   { "unset",            CT_FUNCTION,     LANG_PHP																	  },
   { "unsigned",         CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "use",				 CT_USE,		  LANG_PHP																	  },
   { "ushort",           CT_TYPE,         LANG_CS | LANG_D                                                            },
   { "using",            CT_USING,        LANG_CPP | LANG_CS | LANG_VALA                                              },
   { "var",              CT_TYPE,         LANG_VALA | LANG_ECMA | LANG_PHP                                            },
   { "version",          CT_D_VERSION,    LANG_D                                                                      },
   { "virtual",          CT_QUALIFIER,    LANG_CPP | LANG_CS | LANG_VALA                                              },
   { "void",             CT_TYPE,         LANG_ALLC                                                                   },
   { "volatile",         CT_QUALIFIER,    LANG_C | LANG_CPP | LANG_CS | LANG_JAVA | LANG_ECMA                         },
   { "volatile",         CT_VOLATILE,     LANG_D                                                                      },
   { "wchar",            CT_TYPE,         LANG_D                                                                      },
   { "wchar_t",          CT_TYPE,         LANG_C | LANG_CPP                                                           },
   { "weak",             CT_QUALIFIER,    LANG_VALA                                                                   },
   { "where",            CT_WHERE,        LANG_CPPNET | LANG_CS                                                       },
   { "while",            CT_WHILE,        LANG_ALL                                                                    }, // PAWN
   { "with",             CT_D_WITH,       LANG_D | LANG_ECMA                                                          },
   { "xor",              CT_SARITH,       LANG_C | LANG_CPP | LANG_PHP                                                },
   { "xor_eq",           CT_SASSIGN,      LANG_C | LANG_CPP                                                           },
};



void init_keywords()
{
}

/**
 * Compares two chunk_tag_t entries using strcmp on the strings
 *
 * @param p1   The 'left' entry
 * @param p2   The 'right' entry
 */
static int kw_compare(const void *p1, const void *p2)
{
   const chunk_tag_t *t1 = (const chunk_tag_t *)p1;
   const chunk_tag_t *t2 = (const chunk_tag_t *)p2;

   return(strcmp(t1->tag, t2->tag));
}


#if 0

/**
 * Compares two chunk_tag_t entries using strncmp on the strings;
 * where a full match is one where the entire length of the probe
 * matches the same length of key AND the next character of key
 * after this length is a NON-keyword entity.
 *
 * @param p1   The 'key' entry
 * @param p2   The 'probe' entry
 */
static int kw_compare_ex(const void *p1, const void *p2) /* [i_a] */
{
   const chunk_tag_t *t1 = (const chunk_tag_t *)p1;
   const chunk_tag_t *t2 = (const chunk_tag_t *)p2;
	int len = (int)strlen(t2->tag);
	int ret;

	ret = strncmp(t1->tag, t2->tag, len);
	if (!ret)
	{
	/* a match so far means the key is at least as long as the probe;
       now check the next character in there for a possible full match */
        if (!t1->tag[len] || !CharTable::IsKw2(t1->tag[len]))
		ret = 0;
	else
    		ret = 1; /* key > probe; return sign cf. strcmp() */
	}
#if 0
	printf("--- comparing '%40.40s' with '%s' --> %d\n", t1->tag, t2->tag, ret);
#endif
	return ret;
}

#endif


bool keywords_are_sorted(void)
{
   int  idx;
   bool retval = true;

   for (idx = 1; idx < (int)ARRAY_SIZE(keywords); idx++)
   {
      if (kw_compare(&keywords[idx - 1], &keywords[idx]) > 0)
      {
         LOG_FMT(LERR, "%s: bad sort order at idx %d, words '%s' and '%s'\n",
                 __func__, idx - 1, keywords[idx - 1].tag, keywords[idx].tag);
         retval = false;
      }
   }
   return(retval);
}


/**
 * Adds a keyword to the list of dynamic keywords
 *
 * @param tag        The tag (string) must be zero terminated
 * @param type       The type, usually CT_TYPE
 */
void add_keyword(const char *tag, c_token_t type)
{
   string ss = tag;

   /* See if the keyword has already been added */
   dkwmap::iterator it = dkwm.find(ss);
   if (it != dkwm.end())
   {
      LOG_FMT(LDYNKW, "%s: changed '%s' to %d\n", __func__, tag, type);
      (*it).second = type;
      return;
   }

   /* Insert the keyword */
   dkwm.insert(dkwmap::value_type(ss, type));
   LOG_FMT(LDYNKW, "%s: added '%s' as %d\n", __func__, tag, type);
}


/**
 * Backs up to the first string match in keywords.
 */
static const chunk_tag_t *kw_static_first(const chunk_tag_t *tag)
{
   const chunk_tag_t *prev = tag - 1;

   while ((prev >= &keywords[0]) && (strcmp(prev->tag, tag->tag) == 0))
   {
      tag = prev;
      prev--;
   }
   //fprintf(stderr, "first:%s -", tag->tag);
   return(tag);
}


static const chunk_tag_t *kw_static_match(const chunk_tag_t *tag)
{
   bool              in_pp = ((cpd.in_preproc != CT_NONE) && (cpd.in_preproc != CT_PP_DEFINE));
   bool              pp_iter;
   const chunk_tag_t *iter;

   for (iter = kw_static_first(tag);
        iter < &keywords[ARRAY_SIZE(keywords)];
        iter++)
   {
      //fprintf(stderr, " check:%s", iter->tag);
      pp_iter = (iter->lang_flags & FLAG_PP) != 0;
      if ((strcmp(iter->tag, tag->tag) == 0) &&
          ((cpd.lang_flags & iter->lang_flags) != 0) &&
          (in_pp == pp_iter))
      {
         //fprintf(stderr, " match:%s", iter->tag);
         return(iter);
      }
   }
   return(NULL);
}


/**
 * Search first the dynamic and then the static table for a matching keyword
 *
 * @param word    Pointer to the text -- zero terminated, though probably NOT at len
 * @param len     The length of the text (WARNING: this function will scan
 *                beyond the given len when trying to match dynamic, i.e.
 *                user-specified 'keywords' to a maximum width of strlen(word)!)
 * @return        NULL (no match) or the keyword entry
 */
const chunk_tag_t *find_keyword(const char *word, int len)
{
   string            ss(word, len);
   chunk_tag_t       key;
   const chunk_tag_t *p_ret;

   if (len <= 0)
   {
      return NULL;
   }

#if 0 // this was the old code with special support for macro def's in the config file (whitespaces!)

   if (len > (int)(sizeof(buf) - 1))
   {
      LOG_FMT(LNOTE, "%s: keyword too long at %d char (%d max) : %.*s\n",
              __func__, len, (int)sizeof(buf), len, word);
      return(NULL);
   }
   tag.tag = word; /* allow the tail end to be compared as well! */

   /* check the dynamic word list first */
   p_ret = NULL;
   if (wl.p_tags)
   {
      p_ret = (const chunk_tag_t *)bsearch(&tag, wl.p_tags, wl.active,
                                           sizeof(chunk_tag_t), kw_compare_ex);
   }

   if (p_ret == NULL)
   {
	memcpy(buf, word, len); /* [i_a] */
	buf[len] = 0;

	tag.tag = buf; /* just plain keyword match from here */

      /* check the static word list */
      p_ret = (const chunk_tag_t *)bsearch(&tag, keywords, ARRAY_SIZE(keywords),
                                           sizeof(keywords[0]), kw_compare);
      if (p_ret != NULL)
      {
         //fprintf(stderr, "%s: match %s -", __func__, p_ret->tag);
         p_ret = kw_static_match(p_ret);
         //fprintf(stderr, "\n");
      }
   }

   return(p_ret);
 #endif

   /* check the dynamic word list first */
   dkwmap::iterator it = dkwm.find(ss);
   if (it != dkwm.end())
   {
	   static chunk_tag_t tt = {0};
	   static string cts;

	   cts = ss.c_str();  // make sure the string is cloned!
	   tt.tag = cts.c_str();
	   tt.lang_flags = LANG_ALL;
	   tt.type = ((*it).second);
	   return &tt;
   }

   key.tag = ss.c_str();

   /* check the static word list */
   p_ret = (const chunk_tag_t *)bsearch(&key, keywords, ARRAY_SIZE(keywords),
                                        sizeof(keywords[0]), kw_compare);
   if (p_ret != NULL)
   {
      p_ret = kw_static_match(p_ret);
   }
   return p_ret;
}


/**
 * Loads the dynamic keywords from a file
 *
 * @param filename   The path to the file to load
 * @return           SUCCESS or FAILURE
 */
int load_keyword_file(const char *filename)
{
   FILE *pf;
   char buf[512];
   const char *args[3];
   int  argc;
   int  line_no = 0;

   pf = unc_fopen(filename, "r");
   if (pf == NULL)
   {
      LOG_FMT(LERR, "%s: fopen(%s) failed: %s (%d)\n",
              __func__, filename, strerror(errno), errno);
      cpd.error_count++;
      return(FAILURE);
   }

   while (fgets(buf, sizeof(buf), pf) != NULL)
   {
      line_no++;

      argc       = Args::SplitLine(buf, args, ARRAY_SIZE(args) - 1);
      args[argc] = 0;

      if (argc > 0)
      {
         if ((argc == 1) && CharTable::IsKw1(*args[0]))
         {
            add_keyword(args[0], CT_TYPE);
         }
         else
         {
            LOG_FMT(LWARN, "%s:%d Invalid line (starts with '%s')\n",
                    filename, line_no, args[0]);
            cpd.error_count++;
         }
      }
   }

   unc_fclose(pf);
   return(SUCCESS);
}


void output_types(FILE *pfile)
{
   if (dkwm.size() > 0)
   {
      fprintf(pfile, "-== User Types ==-\n");
      for (dkwmap::iterator it = dkwm.begin(); it != dkwm.end(); ++it)
      {
         fprintf(pfile, "%s\n", (*it).first.c_str());
      }
   }
}


void print_keywords(FILE *pfile)
{
   for (dkwmap::iterator it = dkwm.begin(); it != dkwm.end(); ++it)
   {
      c_token_t tt = (*it).second;
      if (tt == CT_TYPE)
      {
         fprintf(pfile, "type %*.s%s\n",
                 cpd.max_option_name_len - 4, " ", (*it).first.c_str());
      }
      else if (tt == CT_MACRO_OPEN)
      {
         fprintf(pfile, "macro-open %*.s%s\n",
                 cpd.max_option_name_len - 11, " ", (*it).first.c_str());
      }
      else if (tt == CT_MACRO_CLOSE)
      {
         fprintf(pfile, "macro-close %*.s%s\n",
                 cpd.max_option_name_len - 12, " ", (*it).first.c_str());
      }
      else if (tt == CT_MACRO_ELSE)
      {
         fprintf(pfile, "macro-else %*.s%s\n",
                 cpd.max_option_name_len - 11, " ", (*it).first.c_str());
      }
      else
      {
         const char *tn = get_token_name(tt);

         fprintf(pfile, "set %s %*.s%s\n", tn,
                 int(cpd.max_option_name_len - (4 + strlen(tn))), " ", (*it).first.c_str());
      }
   }
}


void clear_keyword_file(void)
{
   dkwm.clear();
}


/**
 * Returns the pattern that the keyword needs based on the token
 */
pattern_class get_token_pattern_class(c_token_t tok)
{
   switch (tok)
   {
   case CT_IF:
   case CT_ELSEIF:
   case CT_SWITCH:
   case CT_FOR:
   case CT_WHILE:
   case CT_USING_STMT:
   case CT_LOCK:
   case CT_D_WITH:
   case CT_D_VERSION_IF:
   case CT_D_SCOPE_IF:
      return(PATCLS_PBRACED);

   case CT_ELSE:
      return(PATCLS_ELSE);

   case CT_DO:
   case CT_TRY:
   case CT_FINALLY:
   case CT_BODY:
   case CT_UNITTEST:
   case CT_UNSAFE:
   case CT_VOLATILE:
   case CT_GETSET:
      return(PATCLS_BRACED);

   case CT_CATCH:
   case CT_D_VERSION:
   case CT_DEBUG:
      return(PATCLS_OPBRACED);

   case CT_NAMESPACE:
      return(PATCLS_VBRACED);

   case CT_WHILE_OF_DO:
      return(PATCLS_PAREN);

   case CT_INVARIANT:
      return(PATCLS_OPPAREN);

   default:
      return(PATCLS_NONE);
   }
}

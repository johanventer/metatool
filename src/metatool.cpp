#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>

/*
 * Globals
 */

static const char *keyword_introspect = "Introspect";
static const char *keyword_struct = "struct";
static const char *keyword_enum = "enum";

static bool printAllTokens = false;
static bool generateOutput = true;

/*
 * Utility
 */

#define arrayLength(a) (int)(sizeof(a) / sizeof(a[0]))
#define allocStruct(type) (type *)calloc(1, sizeof(type))

static void
warn(const char* format, ...)
{
    fprintf(stderr, "[WARN] ");
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

static void 
fatal(const char* format, ...)
{
    fprintf(stderr, "[FATAL] ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    exit(-1);
}

static char* 
readFileToString(const char* fileName)
{
    FILE* file = fopen(fileName, "r");
    if (!file)
        fatal("Could not open %s for reading\n", fileName);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* fileString = (char*)malloc(size + 1);
    fread(fileString, size, 1, file);

    fileString[size] = 0;

    return fileString;
}

static inline bool 
isNewline(char c) {
    return (c == '\n') || (c == '\r');
}

static inline bool 
isWhitespace(char c) {
    return (c == ' ') || (c == '\t') || (c == '\f') || (c == '\v');
}

static inline bool 
isAlphabetic(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool 
isDigit(char c) {
    return (c >= '0' && c <= '9');
}

static uint64_t 
fnv1_hash(const char *str, int length) {
   uint64_t hash = 0xcbf29ce484222325;

   for (int i = 0; i < length; i++) {
       hash *= 0x100000001b3;
       hash ^= str[i];
   }

   return hash;
}

struct String {
    int length;
    char *data;
};

struct StringHashEntry {
    uint64_t key;
    String value;
    StringHashEntry *next;
};

static StringHashEntry *stringHash[1024] = {};

static void
stringHashPut(String *str) {
    uint64_t hash = fnv1_hash(str->data, str->length);
    int bucketIndex = hash % 1024;

    bool bucketNotEmpty = stringHash[bucketIndex] != nullptr;

    if (bucketNotEmpty) {
        for (StringHashEntry *entry = stringHash[bucketIndex]; entry; entry = entry->next) {
            if (entry->key == hash) return;
        }
    } 
        
    StringHashEntry *newEntry = allocStruct(StringHashEntry);
    newEntry->key = hash;
    newEntry->value.length = str->length;
    newEntry->value.data = (char *)malloc(str->length);
    memcpy(newEntry->value.data, str->data, str->length);

    if (bucketNotEmpty) {
        newEntry->next = stringHash[bucketIndex];
        stringHash[bucketIndex] = newEntry;
    } else {
        stringHash[bucketIndex] = newEntry;
    }
}

/*
 * Reverse a linked list given a pointer to its first member.
 */
template <typename T>
void
reverse(T **first) {
    T *prev = nullptr; 
    T *current = *first;
    T *next = nullptr; 
    while (current) { 
        next = current->next; 
        current->next = prev; 
        prev = current; 
        current = next; 
    } 
    *first = prev; 
}

/*
 * Tokenizer
 */

struct Tokenizer {
    char* at;
    int line;
    int column;
};

enum TokenType {
    TokenType_Unknown,

    TokenType_LeftBrace,
    TokenType_RightBrace,
    TokenType_LeftParen,
    TokenType_RightParen,
    TokenType_LeftBracket,
    TokenType_RightBracket,
    TokenType_Slash,
    TokenType_Asterisk,
    TokenType_Semicolon,
    TokenType_Colon,
    TokenType_Pound,
    TokenType_Equals,
    TokenType_Period,
    TokenType_Comma,
    TokenType_LeftCaret,
    TokenType_RightCaret,
    TokenType_Plus,
    TokenType_Minus,
    TokenType_Not,
    TokenType_And,
    TokenType_Or,
    TokenType_Tilde,
    TokenType_Question,

    TokenType_Identifier,
    TokenType_String,
    TokenType_Char,
    TokenType_Number,

    TokenType_End
};

const char *tokenTypeNames[] = {
    [TokenType_Unknown] = "TokenType_Unknown",

    [TokenType_LeftBrace] = "TokenType_LeftBrace",
    [TokenType_RightBrace] = "TokenType_RightBrace",
    [TokenType_LeftParen] = "TokenType_LeftParen",
    [TokenType_RightParen] = "TokenType_RightParen",
    [TokenType_LeftBracket] = "TokenType_LeftBracket",
    [TokenType_RightBracket] = "TokenType_RightBracket",
    [TokenType_Slash] = "TokenType_Slash",
    [TokenType_Asterisk] = "TokenType_Asterisk",
    [TokenType_Semicolon] = "TokenType_Semicolon",
    [TokenType_Colon] = "TokenType_Colon",
    [TokenType_Pound] = "TokenType_Pound",
    [TokenType_Equals] = "TokenType_Equals",
    [TokenType_Period] = "TokenType_Period",
    [TokenType_Comma] = "TokenType_Comma",
    [TokenType_LeftCaret] = "TokenType_LeftCaret",
    [TokenType_RightCaret] = "TokenType_RightCaret",
    [TokenType_Plus] = "TokenType_Plus",
    [TokenType_Minus] = "TokenType_Minus",
    [TokenType_Not] = "TokenType_Not",
    [TokenType_And] = "TokenType_And",
    [TokenType_Or] = "TokenType_Or",
    [TokenType_Tilde] = "TokenType_Tilde",
    [TokenType_Question] = "TokenType_Question",

    [TokenType_Identifier] = "TokenType_Identifier",
    [TokenType_String] = "TokenType_String",
    [TokenType_Char] = "TokenType_Char",
    [TokenType_Number] = "TokenType_Number",

    [TokenType_End] = "TokenType_End"
};

struct Token {
    TokenType type;
    int line;
    int column;
    String text;
};

static inline const char *
tokenTypeName(unsigned tokenType) {
    if (tokenType > arrayLength(tokenTypeNames)) return "<<unknown>>";
    return tokenTypeNames[tokenType];
}

static inline bool 
isValid(Tokenizer* tokenizer) {
    return tokenizer->at[0] != '\0';
}

static void 
advance(Tokenizer* tokenizer, int distance = 1) {
    while (isValid(tokenizer) && distance > 0) {
        if (isNewline(tokenizer->at[0])) {
            tokenizer->column = 1;
            tokenizer->line++;
        } else {
            tokenizer->column++;
        }

        tokenizer->at++;
        distance--;
    }
}

static void 
eatWhitespace(Tokenizer* tokenizer) {
    while (isValid(tokenizer) && (isWhitespace(tokenizer->at[0]) | isNewline(tokenizer->at[0]))) {
        advance(tokenizer);
    }
}

static void 
eatLine(Tokenizer* tokenizer) {
    int line = tokenizer->line;
    while (isValid(tokenizer) && tokenizer->line == line) {
        advance(tokenizer);
    }
}

static Token 
getToken(Tokenizer* tokenizer) {
    Token token = {};

#define current tokenizer->at[0]
#define after tokenizer->at[1]
#define CASE1(c, t) case c: token.type = t; advance(tokenizer); break;

resume:
    eatWhitespace(tokenizer);

    token.type = TokenType_Unknown;
    token.line = tokenizer->line;
    token.column = tokenizer->column;
    token.text.data = tokenizer->at;

    switch (tokenizer->at[0]) {
    case '\0':
        token.type = TokenType_End;
        break;

    CASE1('{', TokenType_LeftBrace);
    CASE1('}', TokenType_RightBrace);
    CASE1('(', TokenType_LeftParen);
    CASE1(')', TokenType_RightParen);
    CASE1('[', TokenType_LeftBracket);
    CASE1(']', TokenType_RightBracket);
    CASE1('*', TokenType_Asterisk);
    CASE1(';', TokenType_Semicolon);
    CASE1(':', TokenType_Colon);
    CASE1('=', TokenType_Equals);
    CASE1('.', TokenType_Period);
    CASE1(',', TokenType_Comma);
    CASE1('<', TokenType_LeftCaret);
    CASE1('>', TokenType_RightCaret);
    CASE1('+', TokenType_Plus);
    CASE1('-', TokenType_Minus);
    CASE1('!', TokenType_Not);
    CASE1('&', TokenType_And);
    CASE1('|', TokenType_Or);
    CASE1('~', TokenType_Tilde);
    CASE1('?', TokenType_Question);

    case '#':
        // Skip all compiler directives for now by ignoring the rest of the line
        eatLine(tokenizer);
        goto resume;
        break;

    case '/':
        advance(tokenizer);
        if (current == '/') {
            // C++ comment, ignore the rest of the line
            eatLine(tokenizer);
            goto resume;
        } else if (current == '*') {
            // C comment
            advance(tokenizer);
            while (isValid(tokenizer)) {
                if (current == '*' && after == '/') {
                    advance(tokenizer, 2);
                    break;
                } else {
                    advance(tokenizer);
                }
            }
            goto resume;
        } else {
            token.type = TokenType_Slash;
        }
        break;

    case '\'':
        token.type = TokenType_Char;
        advance(tokenizer);

        while (isValid(tokenizer) && current != '\'') {
            if (current == '\\') {
                // Handle escape sequence by chomping the next character
                advance(tokenizer);
            }
            advance(tokenizer);
        }

        if (!isValid(tokenizer)) {
            fatal("Unterminated character literal, started at %d:%d\n", token.line, token.column);
        }

        advance(tokenizer);
        break;

    case '"':
        token.type = TokenType_String;
        advance(tokenizer);

        while (isValid(tokenizer) && current != '"') {
            if (current == '\\') {
                // Handle escape sequence by chomping the next character
                advance(tokenizer);
            }
            advance(tokenizer);
        }

        if (!isValid(tokenizer)) {
            fatal("Unterminated string literal, started at %d:%d\n", token.line, token.column);
        }

        advance(tokenizer);
        break;

    default:
        if (isAlphabetic(current) || current == '_') {
            token.type = TokenType_Identifier;
            while (isAlphabetic(current) || isDigit(current) || current == '_') {
                advance(tokenizer);
            }
        } else if (isDigit(current)) {
            token.type = TokenType_Number;
            while (isDigit(current) || current == '.') {
                advance(tokenizer);
            }
        } else {
            token.type = TokenType_Unknown;
            advance(tokenizer);
        }
    }

    token.text.length = tokenizer->at - token.text.data;

    if (printAllTokens) {
        printf("[%d:%d] %s: %.*s\n", token.line, token.column, tokenTypeName(token.type), token.text.length, token.text.data);
    }

#undef CASE1
#undef current
#undef next

    return token;
}

static void 
ensureToken(Token *token, TokenType type) {
  if (token->type != type) {
      fatal("[%d, %d] Expected token %s, got \"%.*s\" which is type %s\n", 
              token->line, token->column, tokenTypeName(type), token->text.length, token->text.data, tokenTypeName(token->type));
  }
}

static Token 
requireToken(Tokenizer *tokenizer, TokenType type) {
    Token token = getToken(tokenizer);
    ensureToken(&token, type);
    return token;
}

static bool
tokenMatchesString(Token *token, const char *keyword) {
    int keywordLength = strlen(keyword);
    if (token->text.length != keywordLength) return false;
    return memcmp(token->text.data, keyword, keywordLength) == 0;
}

/*
 * Parser
 */

struct StructMember {
    Token type;
    Token name;
    bool isPointer;
    bool isArray;
    Token arraySize;
    StructMember *next;
};

struct Struct {
    Token name;
    StructMember *firstMember;
    Struct *next;
};

struct EnumMember {
    Token name;
    EnumMember *next;
};

struct Enum {
    Token name;
    EnumMember *firstMember;
    Enum *next;
};

static StructMember*
parseStructMember(Tokenizer *tokenizer, Token *memberType) {
    StructMember *member = allocStruct(StructMember);
    member->type = *memberType;

    Token token = getToken(tokenizer);
    if (token.type == TokenType_Asterisk) {
        member->isPointer = true;
        member->name = requireToken(tokenizer, TokenType_Identifier);
    } else { 
        member->name = token;
    }

    token = getToken(tokenizer);
    if (token.type == TokenType_LeftBracket) {
        member->isArray = true;
        member->arraySize = getToken(tokenizer);
        requireToken(tokenizer, TokenType_RightBracket);
        requireToken(tokenizer, TokenType_Semicolon);
    } else {
        ensureToken(&token, TokenType_Semicolon);
    }
    
    return member;
}

static Struct *
parseStruct(Tokenizer *tokenizer) {
    Struct *new_struct = allocStruct(Struct);

    new_struct->name = requireToken(tokenizer, TokenType_Identifier);

    requireToken(tokenizer, TokenType_LeftBrace);

    for (;;) {
        Token token = getToken(tokenizer);

        if (token.type == TokenType_RightBrace) {
            break;
        }

        stringHashPut(&token.text);

        StructMember *member = parseStructMember(tokenizer, &token);
        if (!new_struct->firstMember) {
            new_struct->firstMember = member;
        } else {
            member->next = new_struct->firstMember;
            new_struct->firstMember = member;
        }
    }
    requireToken(tokenizer, TokenType_Semicolon);

    reverse(&new_struct->firstMember);

    return new_struct;
}

static Enum *
parseEnum(Tokenizer *tokenizer) {
    Enum *new_enum = allocStruct(Enum);

    new_enum->name = requireToken(tokenizer, TokenType_Identifier);

    requireToken(tokenizer, TokenType_LeftBrace);
    Token token = getToken(tokenizer);

    for (;;) {
        if (token.type == TokenType_RightBrace) {
            break;
        }

        EnumMember *member = allocStruct(EnumMember);
        member->name = token;

        token = getToken(tokenizer);
        if (token.type == TokenType_Equals) {
            // Don't care about the enum member value for now
            Token value = getToken(tokenizer);
            if (value.type != TokenType_Identifier && value.type != TokenType_Number) {
                fatal("[%d:%d] Unknown enum value \"%.*s\"\n", value.line, value.column, value.text.length, value.text.data);
            }
            token = getToken(tokenizer);
        } 

        if (!new_enum->firstMember) {
            new_enum->firstMember = member;
        } else {
            member->next = new_enum->firstMember;
            new_enum->firstMember = member;
        }

        if (token.type == TokenType_Comma) {
            token = getToken(tokenizer);
        }
    }
    requireToken(tokenizer, TokenType_Semicolon);

    reverse(&new_enum->firstMember);

    return new_enum;
}

static void 
appendFlag(char *flags, const char *flag, int *index) {
    if (*index != 0) {
        memcpy(flags + *index, " | ", 3);
        *index += 3;
    }
    int flagLength = strlen(flag);
    memcpy(flags + *index, flag, flagLength);
    *index += flagLength;
}

/*
 * Output generation
 */

static void
outputPreamble() {
    printf("#include <stddef.h>\n\n"
           "#define meta_getMemberPtr(s, m) (void *)(((intptr_t)&(s)) + (m)->offset)\n"
		   "#define meta_isArray(m) (((m)->flags & (Meta_StructMember_Flags_Array)) > 0)\n"
		   "#define meta_isPointer(m) (((m)->flags & (Meta_StructMember_Flags_Pointer)) > 0)\n"
           "\n"  
           );
}

static void
outputMetaDefinitions() {
    printf("enum Meta_StructMember_Flags {\n"
           "    Meta_StructMember_Flags_None,\n"
           "    Meta_StructMember_Flags_Array,\n"
           "    Meta_StructMember_Flags_Pointer\n"
           "};\n\n");

    printf("struct Meta_Struct {\n");
    printf("   const char *name;\n"); 
    printf("   int memberCount;\n"); 
    printf("};\n\n");

    printf("struct Meta_StructMember {\n"
           "    const char *name;\n"
           "    Meta_Type type;\n"
           "    int flags;\n"
           "    int arraySize;\n"
           "    size_t offset;\n"
           "};\n\n");

    printf("struct Meta_Enum {\n");
    printf("   const char *name;\n"); 
    printf("   int memberCount;\n"); 
    printf("};\n\n");

    printf("struct Meta_EnumMember {\n"
           "    const char *name;\n"
           "    int value;\n"
           "};\n\n");
}

static void
outputTypesEnum() {
    printf("enum Meta_Type {\n");

    for (int i = 0; i < arrayLength(stringHash); i++) {
        StringHashEntry *entry = stringHash[i];
        for (; entry; entry = entry->next) {
            printf("    Meta_Type_%.*s,\n", entry->value.length, entry->value.data);
        }
    }

    printf("};\n\n");
}

static void 
outputStruct(Struct *s) {
    int numberOfMembers = 0;

    for (StructMember *member = s->firstMember; member; member = member->next) {
        numberOfMembers++;
    }

    printf("Meta_Struct meta_%.*s = { \"%.*s\", %d };\n\n", 
            s->name.text.length, s->name.text.data, s->name.text.length, s->name.text.data, numberOfMembers);

    printf("Meta_StructMember meta_%.*s_members[] = {\n", s->name.text.length, s->name.text.data);

    for (StructMember *member = s->firstMember; member; member = member->next) {
        // TODO: This is stupid
        char flags[512];
        int index = 0;
        bool hasFlags = false;
        if (member->isPointer) {
            hasFlags = true;
            appendFlag(flags, "Meta_StructMember_Flags_Pointer", &index);
        }
        if (member->isArray) {
            hasFlags = true;
            appendFlag(flags, "Meta_StructMember_Flags_Array", &index);
        }
        if (!hasFlags) {
            appendFlag(flags, "Meta_StructMember_Flags_None", &index);
        }
        flags[index] = '\0';

        printf("    { \"%.*s\", Meta_Type_%.*s, %s, %s%.*s, offsetof(%.*s, %.*s) },\n", 
                member->name.text.length, member->name.text.data, 
                member->type.text.length, member->type.text.data,
                flags, !member->isArray ? "0" : "", 
                member->arraySize.text.length, member->arraySize.text.data,
                s->name.text.length, s->name.text.data, member->name.text.length, member->name.text.data);
    }

    printf("};\n\n");

    printf("inline Meta_Struct *meta_get(%.*s *s) {\n"
           "    return &meta_%.*s;\n"
           "}\n\n", 
           s->name.text.length, s->name.text.data, s->name.text.length, s->name.text.data);

    printf("inline Meta_StructMember *meta_getMembers(%.*s *s) {\n"
           "    return meta_%.*s_members;\n"
           "}\n\n", 
           s->name.text.length, s->name.text.data, s->name.text.length, s->name.text.data);
}

static void
outputEnum(Enum *e) {
    int numberOfMembers = 0;

    for (EnumMember *member = e->firstMember; member; member = member->next) {
        numberOfMembers++;
    }

    printf("Meta_Enum meta_%.*s = { \"%.*s\", %d };\n\n", 
            e->name.text.length, e->name.text.data, e->name.text.length, e->name.text.data, numberOfMembers);

    printf("Meta_EnumMember meta_%.*s_members[] = {\n", e->name.text.length, e->name.text.data);
    for (EnumMember *member = e->firstMember; member; member = member->next) {
        printf("    { \"%.*s\", %.*s },\n", member->name.text.length, member->name.text.data, member->name.text.length, member->name.text.data);
    }
    printf("};\n\n");

    printf("const char *meta_%.*s_names[] = {\n", e->name.text.length, e->name.text.data);
    for (EnumMember *member = e->firstMember; member; member = member->next) {
        printf("    [%.*s] = \"%.*s\",\n", member->name.text.length, member->name.text.data, member->name.text.length, member->name.text.data);
    }
    printf("};\n\n");

    printf("inline const char *meta_getName(%.*s value) {\n"
           "    return meta_%.*s_names[value];\n"
           "}\n\n", 
           e->name.text.length, e->name.text.data, e->name.text.length, e->name.text.data);

    printf("inline Meta_Enum *meta_get(%.*s value) {\n"
           "    return &meta_%.*s;\n"
           "}\n\n", 
           e->name.text.length, e->name.text.data, e->name.text.length, e->name.text.data);
    
    printf("inline Meta_EnumMember *meta_getMembers(%.*s value) {\n"
           "    return meta_%.*s_members;\n"
           "}\n\n", 
           e->name.text.length, e->name.text.data, e->name.text.length, e->name.text.data);
}

/*
 * Main
 */

static void
processFile(const char *fileName) {
    char* fileString = readFileToString(fileName);

    Tokenizer tokenizer = {};
    tokenizer.at = fileString;
    tokenizer.line = 1;
    tokenizer.column = 1;

    bool isParsing = true;

    Struct *firstStruct = nullptr;
    Enum *firstEnum = nullptr;

    while (isParsing) {
        Token token = getToken(&tokenizer);

        switch (token.type) {
        case TokenType_End:
            isParsing = false;
            break;

        case TokenType_Unknown:
            warn("[%d:%d] Unknown token \"%.*s\"\n", token.line, token.column, token.text.length, token.text.data);
            break;

        case TokenType_Identifier:
            if (tokenMatchesString(&token, keyword_introspect)) {
                // TODO parameters
                requireToken(&tokenizer, TokenType_LeftParen);
                requireToken(&tokenizer, TokenType_RightParen);

                Token introspectType = requireToken(&tokenizer, TokenType_Identifier);
                if (tokenMatchesString(&introspectType, keyword_struct)) {
                    Struct *s = parseStruct(&tokenizer);
                    if (!firstStruct) {
                        firstStruct = s;
                    } else {
                        s->next = firstStruct;
                        firstStruct = s;
                    }
                    break;
                } else if (tokenMatchesString(&introspectType, keyword_enum)) {
                    Enum *e = parseEnum(&tokenizer);
                    if (!firstEnum) {
                        firstEnum = e;
                    } else {
                        e->next = firstEnum;
                        firstEnum = e;
                    }
                } else {
                    fatal("[%d:%d] Unknown introspection target \"%.*s\"\n", introspectType.line, introspectType.column, 
                            introspectType.text.length, introspectType.text.data);
                }
            }

        default:
            break;
        }
    }

    if (generateOutput) {
        outputPreamble();
        outputTypesEnum();
        outputMetaDefinitions();

        for (Struct *s = firstStruct; s; s = s->next) {
            outputStruct(s);
        }

        for (Enum *e = firstEnum; e; e = e->next) {
            outputEnum(e);
        }
    }
}

int 
main(int argc, char** argv) {
    if (argc < 2) {
      fatal("Usage: %s <filename.cpp>", argv[0]);
    }
  
    processFile(argv[1]);

    return 0;
}

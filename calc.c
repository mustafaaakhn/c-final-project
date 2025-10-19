// Mustafa Akhan 221ADB228
// Compilation: gcc -O2 -Wall -std=c17 -o calc calc.c
// I used AI help for some specific parts, documented below in comments

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

// Maximum buffer size, assignment says Grade 7 needs 10k characters
#define MAX_BUFFER_SIZE 10001 

// Token types to represent different parts of expression
typedef enum {
    TOKEN_EOF = 0,
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_INVALID
} TokenType;

// Each token stores type, position for error reporting, and value for numbers
typedef struct {
    TokenType type;
    long long pos;    // 1-based character position
    long long value;
} Token;

// Parser result has the computed value and position for tracking errors
typedef struct {
    long long value;
    long long pos;
} Result;

// Global variables for parser state
static char* g_buffer = NULL;
static long long g_pos = 0;
static long long g_char_index = 1;
static long long g_buffer_len = 0;
static long long g_error_pos = 0;
static Token g_current_token;

// Records first error position we find
void fail(long long pos) {
    if (g_error_pos == 0) {
        g_error_pos = pos;
    }
}

// Creates output directory, handles case when it already exist
// I asked AI help. Prompt: how create directory C that not fail if already exists
static int safe_mkdir(const char* path) {
    if (mkdir(path, 0775) == 0) {
        return 0;
    }
    
    if (errno == EEXIST) {
        return 0;
    }

    perror("ERROR: Could not create output directory");
    return -1;
}

// Gets base filename without extension for output naming
// I asked AI help. Prompt: C function get filename without extension
static void get_basename_no_ext(const char* path, char* buffer, size_t size) {
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    
    if (last_backslash > last_slash) {
        last_slash = last_backslash;
    }

    const char* basename_start = last_slash ? last_slash + 1 : path;
    
    strncpy(buffer, basename_start, size - 1);
    buffer[size - 1] = '\0';
    
    char* last_dot = strrchr(buffer, '.');
    if (last_dot) {
        *last_dot = '\0';
    }
}

// Reads input file into memory buffer
static int read_file_to_buffer(const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        perror("ERROR: Could not open input file");
        return -1;
    }

    g_buffer = (char*)malloc(MAX_BUFFER_SIZE);
    if (!g_buffer) {
        fprintf(stderr, "ERROR: Memory allocation failed\n");
        fclose(f);
        return -1;
    }

    g_buffer_len = fread(g_buffer, 1, MAX_BUFFER_SIZE - 1, f);
    if (g_buffer_len == 0 && !feof(f)) {
         fprintf(stderr, "ERROR: File read error\n");
         fclose(f);
         free(g_buffer);
         g_buffer = NULL;
         return -1;
    }
    
    g_buffer[g_buffer_len] = '\0';
    fclose(f);
    return 0;
}

// Tokenizer, breaks input into tokens like numbers, operators etc
// I looked at K&R book chapter 5 for tokenizer ideas
static Token get_next_token() {
    // Skip whitespace and newlines
    while (g_pos < g_buffer_len) {
        char c = g_buffer[g_pos];
        if (isspace(c)) {
            g_pos++;
            g_char_index++;
            continue;
        }
        break; 
    }

    long long token_start_pos = g_char_index;

    if (g_pos >= g_buffer_len) {
        return (Token){TOKEN_EOF, token_start_pos, 0};
    }

    char c = g_buffer[g_pos];

    // Single character tokens
    if (c == '+') { 
        g_pos++; 
        g_char_index++; 
        return (Token){TOKEN_PLUS, token_start_pos, 0}; 
    }
    if (c == '-') { 
        g_pos++; 
        g_char_index++; 
        return (Token){TOKEN_MINUS, token_start_pos, 0}; 
    }
    if (c == '*') { 
        g_pos++; 
        g_char_index++; 
        return (Token){TOKEN_STAR, token_start_pos, 0}; 
    }
    if (c == '/') { 
        g_pos++; 
        g_char_index++; 
        return (Token){TOKEN_SLASH, token_start_pos, 0}; 
    }
    if (c == '(') { 
        g_pos++; 
        g_char_index++; 
        return (Token){TOKEN_LPAREN, token_start_pos, 0}; 
    }
    if (c == ')') { 
        g_pos++; 
        g_char_index++; 
        return (Token){TOKEN_RPAREN, token_start_pos, 0}; 
    }

    // Number parsing, handle negative numbers too
    if (isdigit(c) || (c == '-' && g_pos + 1 < g_buffer_len && isdigit(g_buffer[g_pos + 1]))) {
        const char* startptr = g_buffer + g_pos;
        char* endptr = NULL;
        
        // I asked AI help. Prompt: how use strtoll with overflow check C
        errno = 0;
        long long val = strtoll(startptr, &endptr, 10);
        
        if (errno == ERANGE) {
            fail(token_start_pos);
        }

        long long consumed = endptr - startptr;
        g_pos += consumed;
        g_char_index += consumed;

        return (Token){TOKEN_NUMBER, token_start_pos, val};
    }

    // Invalid character, advance position to avoid infinite loop
    g_pos++; 
    g_char_index++;
    return (Token){TOKEN_INVALID, token_start_pos, 0};
}

// Move to next token if no error happened
static void consume_token() {
    if (g_error_pos == 0) {
        g_current_token = get_next_token();
    }
}

// Forward declarations for parser functions
static Result parse_expr();
static Result parse_term();
static Result parse_primary();

// Macro to check errors and return early if error occured
// I asked AI help. Prompt: C macro for checking error in parser
#define CHECK_ERROR() do { if (g_error_pos != 0) return (Result){0, 0}; } while(0)

// Parses primary expressions - numbers and parenthesized expressions
// Also handles unary minus and plus
static Result parse_primary() {
    CHECK_ERROR();
    Token t = g_current_token;

    int is_negative = 0;
    
    // Handle unary operators
    if (t.type == TOKEN_PLUS) {
        consume_token();
        return parse_primary();
    }
    else if (t.type == TOKEN_MINUS) {
        consume_token();
        is_negative = 1;
        t = g_current_token;
    }

    // Parenthesized expression
    if (t.type == TOKEN_LPAREN) {
        // I asked AI help. Prompt: how implement parentheses recursive descent parser C
        long long paren_pos = t.pos;
        consume_token();
        Result expr = parse_expr();
        CHECK_ERROR();
        
        if (g_current_token.type != TOKEN_RPAREN) {
            fail(paren_pos);
            return (Result){0, 0};
        }
        consume_token();
        
        if (is_negative) {
            expr.value = -expr.value;
        }
        expr.pos = paren_pos;
        return expr;
    }
    // Number token
    else if (t.type == TOKEN_NUMBER) {
        consume_token();
        Result res = (Result){t.value, t.pos};
        if (is_negative) {
            res.value = -res.value;
        }
        return res;
    }
    
    // Unexpected token
    fail(t.pos);
    return (Result){0, 0};
}

// Parses term - handles multiplication and division, higher precedence
// I asked AI help. Prompt: recursive descent parser multiplication division C
static Result parse_term() {
    CHECK_ERROR();
    Result left = parse_primary();
    CHECK_ERROR();

    while (g_current_token.type == TOKEN_STAR || g_current_token.type == TOKEN_SLASH) {
        Token op = g_current_token;
        consume_token();
        Result right = parse_primary();
        CHECK_ERROR();

        if (op.type == TOKEN_STAR) {
            left.value = left.value * right.value;
        } else {
            // Check division by zero
            if (right.value == 0) {
                fail(right.pos);
                return (Result){0, 0};
            }
            left.value = left.value / right.value;
        }
        left.pos = op.pos;
    }
    return left;
}

// Parses expression - handles addition and subtraction, lowest precedence
// I asked AI help. Prompt: recursive descent parser addition subtraction C
static Result parse_expr() {
    CHECK_ERROR();
    Result left = parse_term();
    CHECK_ERROR();

    while (g_current_token.type == TOKEN_PLUS || g_current_token.type == TOKEN_MINUS) {
        Token op = g_current_token;
        consume_token();
        Result right = parse_term();
        CHECK_ERROR();
        
        if (op.type == TOKEN_PLUS) {
            left.value = left.value + right.value;
        } else {
            left.value = left.value - right.value;
        }
        left.pos = op.pos;
    }
    return left;
}

// Main program
int main(int argc, char *argv[]) {
    const char* name_part = "Mustafa_Akhan_221ADB228";
    const char* username = "makhan";
    const char* studentid = "221ADB228";

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input.txt>\n", argv[0]);
        return 1;
    }
    
    const char* input_path = argv[1];

    char base_name[256];
    get_basename_no_ext(input_path, base_name, sizeof(base_name));

    // Create output directory with naming convention required
    char output_dir_name[512];
    snprintf(output_dir_name, sizeof(output_dir_name), "%s_%s_%s", 
             base_name, username, studentid);

    if (safe_mkdir(output_dir_name) != 0) {
        fprintf(stderr, "ERROR: Failed to create output directory '%s'\n", output_dir_name);
        return 1;
    }

    // Construct output file path
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/%s_%s.txt", 
             output_dir_name, base_name, name_part);

    if (read_file_to_buffer(input_path) != 0) {
        return 1;
    }

    // Start parsing
    consume_token();
    Result res = parse_expr();

    // Check for unexpected tokens after expression
    if (g_error_pos == 0 && g_current_token.type != TOKEN_EOF) {
        fail(g_current_token.pos);
    }

    // Write results to output file
    FILE* out_file = fopen(output_path, "w");
    if (!out_file) {
        perror("ERROR: Could not open output file");
        free(g_buffer);
        return 1;
    }

    if (g_error_pos != 0) {
        fprintf(out_file, "ERROR:%lld\n", g_error_pos);
    } else {
        fprintf(out_file, "%lld\n", res.value);
    }

    fclose(out_file);
    free(g_buffer);

    printf("Done. Output: %s\n", output_path);
    return 0;
}

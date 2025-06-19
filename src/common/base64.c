#include "base64.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// Base64编码表
static const char base64_chars[] = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Base64解码表
static const int base64_decode_table[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
    52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-2,-1,-1,
    -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
    -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};

size_t base64_encoded_length(size_t input_length) {
    return ((input_length + 2) / 3) * 4;
}

size_t base64_decoded_length(const char* input, size_t input_length) {
    if (input_length == 0) return 0;
    
    size_t padding = 0;
    if (input[input_length - 1] == '=') padding++;
    if (input_length > 1 && input[input_length - 2] == '=') padding++;
    
    return (input_length * 3) / 4 - padding;
}

int is_base64_char(char c) {
    return (c >= 'A' && c <= 'Z') || 
           (c >= 'a' && c <= 'z') || 
           (c >= '0' && c <= '9') || 
           c == '+' || c == '/' || c == '=';
}

int base64_encode(const unsigned char* input, size_t input_length, 
                  char* output, size_t output_length) {
    if (!input || !output) return -1;
    
    size_t encoded_length = base64_encoded_length(input_length);
    if (output_length < encoded_length + 1) return -1; // +1 for null terminator
    
    size_t i, j;
    for (i = 0, j = 0; i < input_length; ) {
        uint32_t octet_a = i < input_length ? input[i++] : 0;
        uint32_t octet_b = i < input_length ? input[i++] : 0;
        uint32_t octet_c = i < input_length ? input[i++] : 0;
        
        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        
        output[j++] = base64_chars[(triple >> 3 * 6) & 0x3F];
        output[j++] = base64_chars[(triple >> 2 * 6) & 0x3F];
        output[j++] = base64_chars[(triple >> 1 * 6) & 0x3F];
        output[j++] = base64_chars[(triple >> 0 * 6) & 0x3F];
    }
    
    // 添加填充
    int mod = input_length % 3;
    if (mod == 1) {
        output[encoded_length - 2] = '=';
        output[encoded_length - 1] = '=';
    } else if (mod == 2) {
        output[encoded_length - 1] = '=';
    }
    
    output[encoded_length] = '\0';
    return encoded_length;
}

int base64_decode(const char* input, size_t input_length, 
                  unsigned char* output, size_t output_length) {
    if (!input || !output) return -1;
    if (input_length % 4 != 0) return -1;
    
    size_t decoded_length = base64_decoded_length(input, input_length);
    if (output_length < decoded_length) return -1;
    
    size_t i, j;
    for (i = 0, j = 0; i < input_length; ) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_decode_table[(int)input[i++]];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_decode_table[(int)input[i++]];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_decode_table[(int)input[i++]];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_decode_table[(int)input[i++]];
        
        if (sextet_a == -1 || sextet_b == -1 || sextet_c == -1 || sextet_d == -1) {
            return -1; // 无效字符
        }
        
        uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + 
                         (sextet_c << 1 * 6) + (sextet_d << 0 * 6);
        
        if (j < decoded_length) output[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < decoded_length) output[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < decoded_length) output[j++] = (triple >> 0 * 8) & 0xFF;
    }
    
    return decoded_length;
}
#ifndef BASE64_H
#define BASE64_H

#include <stddef.h>

/**
 * Base64编码函数
 * @param input 输入数据
 * @param input_length 输入数据长度
 * @param output 输出缓冲区（需要预先分配足够空间）
 * @param output_length 输出缓冲区大小
 * @return 编码后的数据长度，失败返回-1
 */
int base64_encode(const unsigned char* input, size_t input_length, 
                  char* output, size_t output_length);

/**
 * Base64解码函数
 * @param input 输入的Base64字符串
 * @param input_length 输入字符串长度
 * @param output 输出缓冲区（需要预先分配足够空间）
 * @param output_length 输出缓冲区大小
 * @return 解码后的数据长度，失败返回-1
 */
int base64_decode(const char* input, size_t input_length, 
                  unsigned char* output, size_t output_length);

/**
 * 计算Base64编码后的长度
 * @param input_length 原始数据长度
 * @return 编码后的长度
 */
size_t base64_encoded_length(size_t input_length);

/**
 * 计算Base64解码后的长度
 * @param input Base64字符串
 * @param input_length 字符串长度
 * @return 解码后的长度
 */
size_t base64_decoded_length(const char* input, size_t input_length);

/**
 * 检查字符是否为有效的Base64字符
 * @param c 字符
 * @return 1表示有效，0表示无效
 */
int is_base64_char(char c);

#endif // BASE64_H
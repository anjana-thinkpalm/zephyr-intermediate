
#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>

#include "credentials.h"
#include "parser.h"

#define PARSER_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(parser_stack, PARSER_STACK_SIZE);

static struct k_thread parser_thread;

/*
 * Parser thread:
 *  - Processes one valid command.
 *  - Processes two attack commands that attempt to read the private key.
 *
 * The parser and credentials implementations remain unchanged.
 */
static void parser_entry(void *attack_index, void *p2, void *p3)
{
    char packets[3][32];

    strcpy(packets[0], "get=0");

    snprintf(packets[1], sizeof(packets[1]),
             "get=%ld", (long)attack_index);

    snprintf(packets[2], sizeof(packets[2]),
             "get=%ld", (long)attack_index + 1);

    for (int i = 0; i < ARRAY_SIZE(packets); i++) {
        printk("parser: received '%s'\n", packets[i]);
        parse_command(packets[i]);
    }
}

int main(void)
{
    /*
     * Calculate the index corresponding to the private key.
     * This calculation is performed by the privileged main thread.
     */
    long attack_index =
        ((const char *)credentials_key_addr() -
         (const char *)parser_settings_addr()) / SETTING_SIZE;

    /*
     * Run the parser in user mode so the MPU restricts its access
     * to protected memory.
     */
    k_thread_create(&parser_thread,
                    parser_stack,
                    PARSER_STACK_SIZE,
                    parser_entry,
                    (void *)attack_index,
                    NULL,
                    NULL,
                    5,
                    K_USER,
                    K_NO_WAIT);

    return 0;
}

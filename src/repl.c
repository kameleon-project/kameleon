/* Copyright (c) 2017 Kaluma
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "tty.h"
#include "flash.h"
#include "repl.h"
#include "runtime.h"
#include "system.h"
#include "jerryscript.h"
#include "utils.h"
#include "kaluma_config.h"
#include "ymodem.h"

// --------------------------------------------------------------------------
// FORWARD DECLARATIONS
// --------------------------------------------------------------------------

static void cmd_echo(km_repl_state_t *state, char *arg);
static void cmd_reset(km_repl_state_t *state);
static void cmd_flash(km_repl_state_t *state, char *arg);
static void cmd_load(km_repl_state_t *state);
static void cmd_mem(km_repl_state_t *state);
static void cmd_gc(km_repl_state_t *state);
static void cmd_hi(km_repl_state_t *state);
static void cmd_help(km_repl_state_t *state);

// --------------------------------------------------------------------------
// PRIVATE VARIABLES
// --------------------------------------------------------------------------

/**
 * TTY handle for REPL
 */
static km_io_tty_handle_t tty;

/**
 * REPL state
 */
static km_repl_state_t state;

// --------------------------------------------------------------------------
// PRIVATE FUNCTIONS
// --------------------------------------------------------------------------

/**
 * Inject a char to REPL
 */
static void tty_read_cb(uint8_t *buf, size_t len) {
  if (state.handler != NULL) {
    (*state.handler)(&state, buf, len); /* call handler */
  }
}

/**
 * Push a command to history
 */
static void history_push(char *cmd) {
  if (state.history_size < MAX_COMMAND_HISTORY) {
    state.history[state.history_size] = cmd;
    state.history_size++;
  } else {
    // free memory of history[0]
    free(state.history[0]);
    // Shift history array to left (e.g. 1 to 0, 2 to 1, ...)
    for (int i = 0; i < (state.history_size - 1); i++) {
      state.history[i] = state.history[i + 1];
    }
    // Put to the last of history
    state.history[state.history_size - 1] = cmd;
  }
  state.history_position = state.history_size;
}

/**
 * Run the REPL command in the buffer
 */
static void run_command() {
  if (state.buffer_length > 0) {

    /* copy buffer to data */
    char *data = malloc(state.buffer_length + 1);
    state.buffer[state.buffer_length] = '\0';
    strcpy(data, state.buffer);
    state.buffer_length = 0;
    state.position = 0;

    /* push to history */
    history_push(data);

    /* tokenize command */
    char *tokenv[5];
    unsigned int tokenc = 0;
    tokenv[0] = strtok(state.buffer, " ");
    while (tokenv[tokenc] != NULL && tokenc < 5) {
      tokenc++;
      tokenv[tokenc] = strtok (NULL, " ");
    }
    /* run command */
    if (strcmp(tokenv[0], ".echo") == 0) {
      if (tokenv[1] != NULL)
      {
        cmd_echo(&state, tokenv[1]);
      }
    } else if (strcmp(tokenv[0], ".reset") == 0) {
      cmd_reset(&state);
    } else if (strcmp(tokenv[0], ".flash") == 0) {
      if (tokenv[1] == NULL)
      {
        tokenv[1] = "";
      }
      cmd_flash(&state, tokenv[1]);
    } else if (strcmp(tokenv[0], ".load") == 0) {
      cmd_load(&state);
    } else if (strcmp(tokenv[0], ".mem") == 0) {
      cmd_mem(&state);
    } else if (strcmp(tokenv[0], ".gc") == 0) {
      cmd_gc(&state);
    } else if (strcmp(tokenv[0], ".hi") == 0) {
      cmd_hi(&state);
    } else if (strcmp(tokenv[0], ".help") == 0) {
      cmd_help(&state);
    } else { /* unknown command */
      km_repl_set_output(KM_REPL_OUTPUT_ERROR);
      km_repl_printf("Unknown command: %s\r\n", tokenv[0]);
      km_repl_set_output(KM_REPL_OUTPUT_NORMAL);
    }
  } else {
    km_repl_set_output(KM_REPL_OUTPUT_NORMAL);
  }
}

/**
 * Evaluate JS code in the buffer
 */
static void run_code() {
  if (state.buffer_length > 0) {

    /* copy buffer to data */
    char *data = malloc(state.buffer_length + 1);
    state.buffer[state.buffer_length] = '\0';
    strcpy(data, state.buffer);
    state.buffer_length = 0;
    state.position = 0;

    /* push to history */
    history_push(data);

    /* evaluate code */
    jerry_value_t parsed_code = jerry_parse(NULL, 0, (const jerry_char_t *) data, strlen(data), JERRY_PARSE_STRICT_MODE);
    if (jerry_value_is_error(parsed_code)) {
      jerryxx_print_error(parsed_code, false);
    } else {
      jerry_value_t ret_value = jerry_run(parsed_code);
      if (jerry_value_is_error(ret_value)) {
        jerryxx_print_error(ret_value, false);
      } else {
        km_repl_pretty_print(0, 3, ret_value);
        km_repl_println();
      }
      jerry_release_value(ret_value);
    }
    jerry_release_value(parsed_code);
  }
}

/**
 * Move cursor to state.position in consideration with state.width
 */
static void set_cursor_to_position() {
  int horz = (state.position + 2) % state.width;
  int vert = (state.position + 2) / state.width;
  if (horz > 0) {
    if (vert > 0) {
      km_repl_printf("\033[u\033[%dC\033[%dB", horz, vert);
    } else {
      km_repl_printf("\033[u\033[%dC", horz);
    }
  } else {
    if (vert > 0) {
      km_repl_printf("\033[u\033[%dB", vert);
    } else {
      km_repl_printf("\033[u");
    }
  }
}

/**
 * Handler for normal mode
 */
static void handle_normal(char ch) {
  switch (ch) {
    case '\r': /* carrage return */
      if (state.echo) {
        km_repl_printf("\r\n");
      }
      if (state.buffer_length > 0 && state.buffer[0] == '.') {
        run_command();
      } else {
        run_code();
      }
      km_repl_print_prompt();
      break;
    case 0x01: /* Ctrl + A */
      state.position = 0;
      set_cursor_to_position();
      break;
    case 0x05: /* Ctrl + E */
      state.position = state.buffer_length;
      set_cursor_to_position();
      break;
    case 0x08: /* backspace */
    case 0x7f: /* also backspace in some terminal */
      if (state.buffer_length > 0 && state.position > 0) {
        state.position--;
        for (int i = state.position; i < state.buffer_length - 1; i++) {
          state.buffer[i] = state.buffer[i + 1];
        }
        state.buffer_length--;
        state.buffer[state.buffer_length] = '\0';
        if (state.echo) {
          km_repl_printf("\033[D\033[K\033[J");
          km_repl_printf("\033[u> %s", state.buffer);
          set_cursor_to_position();
        }
      }
      break;
    case 0x1b: /* escape char */
      state.mode = KM_REPL_MODE_ESCAPE;
      state.escape_length = 0;
      break;
    default:
      // check buffer overflow
      if (state.buffer_length < (MAX_BUFFER_LENGTH - 1)) {
        if (state.position == state.buffer_length) {
          state.buffer[state.position] = ch;
          state.buffer_length++;
          state.position++;
          if (state.echo) {
            km_repl_putc(ch);
          }
        } else {
          for (int i = state.buffer_length; i > state.position; i--) {
            state.buffer[i] = state.buffer[i - 1];
          }
          state.buffer[state.position] = ch;
          state.buffer_length++;
          state.position++;
          state.buffer[state.buffer_length] = '\0';
          if (state.echo) {
            km_repl_printf("\033[u> %s", state.buffer);
            set_cursor_to_position();
          }
        }
      } else {
        km_repl_set_output(KM_REPL_OUTPUT_ERROR);
        km_repl_printf("%s\r\n", "REPL buffer overflow");
        km_repl_set_output(KM_REPL_OUTPUT_NORMAL);
      }
      break;
  }
}

/**
 * Handle char in escape sequence
 */
static void handle_escape(char ch) {
  state.escape[state.escape_length] = ch;
  state.escape_length++;

  // if ch is last char (a-zA-Z) of escape sequence
  if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
    state.mode = KM_REPL_MODE_NORMAL;

    /* up key */
    if (state.escape_length == 2 && state.escape[0] == 0x5b && state.escape[1] == 0x41) {
      if (state.history_position > 0) {
        state.history_position--;
        char *cmd = state.history[state.history_position];
        km_repl_printf("\33[2K\r> %s", cmd);
        strcpy(state.buffer, cmd);
        state.buffer_length = strlen(cmd);
        state.position = state.buffer_length;
      }

    /* down key */
    } else if (state.escape_length == 2 && state.escape[0] == 0x5b && state.escape[1] == 0x42) {
      if (state.history_position == state.history_size) {
        /* do nothing */
      } else if (state.history_position == (state.history_size - 1)) {
        state.history_position++;
        km_repl_printf("\33[2K\r> ");
        state.buffer_length = 0;
        state.position = 0;
      } else {
        state.history_position++;
        char *cmd = state.history[state.history_position];
        km_repl_printf("\33[2K\r> %s", cmd);
        strcpy(state.buffer, cmd);
        state.buffer_length = strlen(cmd);
        state.position = state.buffer_length;
      }

    /* left key */
    } else if (state.escape_length == 2 && state.escape[0] == 0x5b && state.escape[1] == 0x44) {
      if (state.position > 0) {
        state.position--;
        set_cursor_to_position();
      }

    /* right key */
    } else if (state.escape_length == 2 && state.escape[0] == 0x5b && state.escape[1] == 0x43) {
      if (state.position < state.buffer_length) {
        state.position++;
        set_cursor_to_position();
      }

    /* receive cursor position and update screen width */
    } else if (state.escape[state.escape_length - 1] == 'R') {
      int pos = 0;
      for (int i = 0; i < state.escape_length; i++) {
        if (state.escape[i] == ';') {
          pos = i + 1;
          break;
        }
      }
      state.escape[state.escape_length - 1] = '\0';
      state.width = atoi(state.escape + pos);

    // Run original escape sequence
    } else {
      km_repl_putc('\033');
      for (int i = 0; i < state.escape_length; i++) {
        km_repl_putc(state.escape[i]);
      }
    }
  } else if (ch == 0x7e) { /* special key */
    state.mode = KM_REPL_MODE_NORMAL;

    /* delete key */
    if (state.escape_length == 3 && state.escape[0] == 0x5b && state.escape[1] == 0x33) {
      if (state.buffer_length > 0 && state.position < state.buffer_length) {
        for (int i = state.position; i < state.buffer_length; i++) {
          state.buffer[i] = state.buffer[i + 1];
        }
        state.buffer_length--;
        state.buffer[state.buffer_length] = '\0';
        if (state.echo) {
          km_repl_printf("\033[K\033[J");
          km_repl_printf("\033[u> %s", state.buffer);
          set_cursor_to_position();
        }
      }
    }
    state.escape_length = 0;
  }
}

/**
 * Default handler
 */
static void default_handler(km_repl_state_t *state, uint8_t *buf, size_t len) {
  for (int i = 0; i < len; i++) {
    char ch = buf[i];
    switch (state->mode) {
      case KM_REPL_MODE_NORMAL:
        handle_normal(ch);
        break;
      case KM_REPL_MODE_ESCAPE:
        handle_escape(ch);
        break;
    }
  }
}
#if 0 //Never used.
/**
 * Change handler
 */
static void set_handler(repl_handler_t handler) {
  if (handler != NULL) {
    state.handler = handler;
  } else {
    state.handler = &default_handler;
  }
}
#endif
/**
 * .echo command
 */
static void cmd_echo(km_repl_state_t *state, char *arg) {
  if (strcmp(arg, "on") == 0) {
    state->echo = true;
  } else if (strcmp(arg, "off") == 0) {
    state->echo = false;
  }
}

/**
 * .reset command
 */
static void cmd_reset(km_repl_state_t *state) {
  km_runtime_cleanup();
  km_runtime_init(false, false);
}

static size_t bytes_remained = 0;

static int header_cb(uint8_t *file_name, size_t file_size) {
  km_flash_program_begin();
  bytes_remained = file_size;
  return 0;
}

static int packet_cb(uint8_t *data, size_t len) {
  if (bytes_remained < len) {
    len = bytes_remained;
    bytes_remained = 0;
  } else {
    bytes_remained = bytes_remained - len;
  }
  km_flash_status_t status = km_flash_program(data, len);
  if (status == KM_FLASH_SUCCESS) {
    return 0;
  } else {
    return -1;
  }
  return 0;
}

static void footer_cb() {
  km_flash_program_end();
  bytes_remained = 0;
}

/**
 * .flash command
 */
static void cmd_flash(km_repl_state_t *state, char *arg) {
  /* erase flash */
  if (strcmp(arg, "-e") == 0) {
    km_flash_clear();
    km_repl_printf("Flash has erased\r\n");

  /* get total size of flash */
  } else if (strcmp(arg, "-t") == 0) {
    uint32_t size = km_flash_size();
    km_repl_printf("%u\r\n", size);

  /* get data size in flash */
  } else if (strcmp(arg, "-s") == 0) {
    uint32_t data_size = km_flash_get_data_size();
    km_repl_printf("%u\r\n", data_size);

  /* read data from flash */
  } else if (strcmp(arg, "-r") == 0) {
    uint32_t sz = km_flash_get_data_size();
    uint8_t *ptr = km_flash_get_data();
    for (int i = 0; i < sz; i++) {
      if (ptr[i] == '\n') { /* convert "\n" to "\r\n" */
        km_repl_putc('\r');
      }
      km_repl_putc(ptr[i]);
    }
    km_repl_println();
    km_flash_free_data(ptr);
  /* write a file to flash via Ymodem */
  } else if (strcmp(arg, "-w") == 0) {
    state->ymodem_state = 1; // transfering
    km_tty_printf("Transfer a file via Ymodem... (press 'a' to abort)\r\n");
    km_io_tty_read_stop(&tty);
    km_ymodem_status_t result = km_ymodem_receive(header_cb, packet_cb, footer_cb);
    km_io_tty_read_start(&tty, tty_read_cb);
    km_delay(500);
    switch (result) {
      case KM_YMODEM_OK:
        km_tty_printf("\r\nDone.\r\n");
        break;
      case KM_YMODEM_LIMIT:
        km_tty_printf("\r\nThe file size is too large.\r\n");
        break;
      case KM_YMODEM_DATA:
        km_tty_printf("\r\nVerification failed.\r\n");
        break;
      case KM_YMODEM_ABORT:
        km_tty_printf("\r\nAborted.\r\n");
        break;
      default:
        km_tty_printf("\r\nFailed to receive.\r\n");
        break;
    }
    state->ymodem_state = 0; // stopped
  /* no option is given */
  } else {
    km_repl_printf(".flash command options:\r\n");
    km_repl_printf("-w\tWrite user code (file) to flash via Ymodem.\r\n");
    km_repl_printf("-e\tErase the user code in flash.\r\n");
    km_repl_printf("-t\tPrint total size of flash for user code.\r\n");
    km_repl_printf("-s\tPrint the size of the user code.\r\n");
    km_repl_printf("-r\tPrint the user code in textual format.\r\n");
  }
}

/**
 * .load command
 */
static void cmd_load(km_repl_state_t *state) {
  km_runtime_cleanup();
  km_runtime_init(true, false);
}

/**
 * .mem command
 */
static void cmd_mem(km_repl_state_t *state) {
  jerry_heap_stats_t stats = {0};
  bool stats_ret = jerry_get_memory_stats (&stats);
  if (stats_ret) {
    km_repl_printf("total: %u, occupied: %u, peak: %u\r\n", stats.size, stats.allocated_bytes, stats.peak_allocated_bytes);
  } else {
    km_repl_printf("Mem stat feature is not enabled.\r\n");
  }
}

/**
 * .gc command
 */
static void cmd_gc(km_repl_state_t *state) {
  jerry_gc(JERRY_GC_PRESSURE_HIGH);
}

/**
 * .hi command
 */
static void cmd_hi(km_repl_state_t *state) {
  km_repl_printf("/---------------------------\\\r\n");
  km_repl_printf("|                  ____     |\r\n");
  km_repl_printf("|     /----_______/    \\    |\r\n");
  km_repl_printf("|    /               O  \\   |\r\n");
  km_repl_printf("|   /               _____\\  |\r\n");
  km_repl_printf("|  |  /------__ ___ ____/   |\r\n");
  km_repl_printf("|  | | /``\\   //   \\\\       |\r\n");
  km_repl_printf("|  | \\ @`\\ \\  W     W       |\r\n");
  km_repl_printf("|   \\ \\__/ / ***************|\r\n");
  km_repl_printf("|    \\____/     ************|\r\n");
  km_repl_printf("|                       ****|\r\n");
  km_repl_printf("\\---------------------------/\r\n");
  km_repl_printf("\r\n");
  km_repl_printf("Welcome to Kaluma!\r\n");
  km_repl_printf("%s %s\r\n", "Version:", KALUMA_VERSION);
  km_repl_printf("For more info: https://kaluma.io\r\n");
  km_repl_println();
}

/**
 * .help command
 */
static void cmd_help(km_repl_state_t *state) {
  km_repl_printf(".echo\tEcho on/off.\r\n");
  km_repl_printf(".reset\tReset JavaScript runtime context.\r\n");
  km_repl_printf(".flash\tCommands for the internal flash.\r\n");
  km_repl_printf(".load\tLoad user code from the internal flash.\r\n");
  km_repl_printf(".mem\tHeap memory status.\r\n");
  km_repl_printf(".gc\tPerform garbage collection.\r\n");
  km_repl_printf(".hi\tPrint welcome message.\r\n");
  km_repl_printf(".help\tPrint this help message.\r\n");
}

// --------------------------------------------------------------------------
// PUBLIC FUNCTIONS
// --------------------------------------------------------------------------

/**
 * Initialize the REPL
 */
void km_repl_init() {
  km_io_tty_init(&tty);
  km_io_tty_read_start(&tty, tty_read_cb);
  state.mode = KM_REPL_MODE_NORMAL;
  state.echo = true;
  state.buffer_length = 0;
  state.position = 0;
  state.width = 80;
  state.escape_length = 0;
  state.history_size = 0;
  state.history_position = 0;
  state.handler = &default_handler;
  state.ymodem_state = 0;
  cmd_hi(NULL);
}

km_repl_state_t *km_get_repl_state() {
  return &state;
}

void km_repl_set_output(km_repl_output_t output) {
  switch (output) {
    case KM_REPL_OUTPUT_NORMAL:
      km_tty_printf("\33[0m"); /* set to normal color */
      break;
    case KM_REPL_OUTPUT_INFO:
      km_tty_printf("\33[90m"); /* set to dark gray color */
      break;
    case KM_REPL_OUTPUT_ERROR:
      km_tty_printf("\33[31m"); /* set to red color */
      break;
  }
}

static void km_repl_pretty_print_indent(uint8_t indent) {
  for (uint8_t i = 0; i < indent; i++)
    km_tty_putc(' ');
}

struct km_repl_pretty_print_object_foreach_data {
  uint8_t indent;
  uint8_t depth;
  uint16_t count;
};

static bool km_repl_pretty_print_object_foreach_count(const jerry_value_t prop_name,
    const jerry_value_t prop_value, void *user_data_p) {
  if (jerry_value_is_string (prop_name)) {
    struct km_repl_pretty_print_object_foreach_data *data =
        (struct km_repl_pretty_print_object_foreach_data *) user_data_p;
    data->count++;
  }
  return true;
}

static bool km_repl_pretty_print_object_foreach(const jerry_value_t prop_name,
    const jerry_value_t prop_value, void *user_data_p) {
  if (jerry_value_is_string (prop_name)) {
    jerry_char_t buf[128];
    jerry_size_t len = jerry_substring_to_char_buffer (prop_name, 0, 127, buf, 127);
    buf[len] = '\0';
    struct km_repl_pretty_print_object_foreach_data *data =
        (struct km_repl_pretty_print_object_foreach_data *) user_data_p;
    km_repl_pretty_print_indent(data->indent + 2);
    km_tty_printf((const char *) buf);
    km_tty_printf(": ");
    km_repl_pretty_print(data->indent + 2, data->depth - 1, prop_value);
    if (data->count > 1) {
      km_tty_printf(",");
    }
    km_tty_printf("\r\n");
    data->count--;
  }
  return true;
}

void km_repl_pretty_print(uint8_t indent, uint8_t depth, jerry_value_t value) {
  if (depth < 0) {
    return;
  } else if (depth == 0) {
    if (jerry_value_is_abort(value)) {
      km_tty_printf("\33[31m"); // red
      km_tty_printf("[Abort]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_array(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[Array]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_typedarray(value)) {
      km_tty_printf("\33[96m"); // cyan
      jerry_typedarray_type_t type = jerry_get_typedarray_type(value);
      switch (type) {
      case JERRY_TYPEDARRAY_UINT8:
        km_tty_printf("[Uint8Array]");
        break;
      case JERRY_TYPEDARRAY_UINT8CLAMPED:
        km_tty_printf("[Uint8ClampedArray]");
        break;
      case JERRY_TYPEDARRAY_INT8:
        km_tty_printf("[Int8Array]");
        break;
      case JERRY_TYPEDARRAY_UINT16:
        km_tty_printf("[Uint16Array]");
        break;
      case JERRY_TYPEDARRAY_INT16:
        km_tty_printf("[Int16Array]");
        break;
      case JERRY_TYPEDARRAY_UINT32:
        km_tty_printf("[Uint32Array]");
        break;
      case JERRY_TYPEDARRAY_INT32:
        km_tty_printf("[Int32Array]");
        break;
      case JERRY_TYPEDARRAY_FLOAT32:
        km_tty_printf("[Float32Array]");
        break;
      case JERRY_TYPEDARRAY_FLOAT64:
        km_tty_printf("[Float64Array]");
        break;
      default:
        km_tty_printf("[TypedArray]");
        break;
      }
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_arraybuffer(value)) {
      jerry_length_t len = jerry_get_arraybuffer_byte_length(value);
      km_tty_printf("ArrayBuffer { byteLength:");
      km_tty_printf("\33[95m"); // magenta
      km_tty_printf("%d", len);
      km_tty_printf("\33[0m");
      km_tty_printf("}");
    } else if (jerry_value_is_boolean(value)) {
      km_tty_printf("\33[95m"); // magenta
      km_repl_print_value(value);
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_constructor(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[Function]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_dataview(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[DataView]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_error(value)) {
      km_tty_printf("\33[31m"); // red
      km_tty_printf("[Error]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_function(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[Function]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_number(value)) {
      km_tty_printf("\33[95m"); // magenda
      km_repl_print_value(value);
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_null(value)) {
      km_tty_printf("\33[90m"); // dark gray
      km_tty_printf("null");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_promise(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[Promise]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_object(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[Object]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_string(value)) {
      km_tty_printf("\33[93m"); // yellow
      km_tty_printf("'");
      km_repl_print_value(value);
      km_tty_printf("'");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_symbol(value)) {
      km_tty_printf("\33[96m"); // cyan
      km_tty_printf("[Symbol]");
      km_tty_printf("\33[0m");
    } else if (jerry_value_is_undefined(value)) {
      km_tty_printf("\33[90m"); // dark gray
      km_tty_printf("undefined");
      km_tty_printf("\33[0m");
    }
  } else {
    if (jerry_value_is_abort(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_array(value)) {
      uint32_t len = jerry_get_array_length (value);
      km_tty_printf("[");
      if (len > 0) {
        km_tty_printf("\r\n");
        for (int i = 0; i < len; i++) {
          jerry_value_t item = jerry_get_property_by_index(value, i);
          km_repl_pretty_print_indent(indent + 2);
          km_repl_pretty_print(indent + 2, depth - 1, item);
          jerry_release_value(item);
          if (i < len - 1) km_tty_printf(",");
          km_tty_printf("\r\n");
        }
        km_repl_pretty_print_indent(indent);
      }
      km_tty_printf("]");
    } else if (jerry_value_is_typedarray(value)) {
      jerry_typedarray_type_t type = jerry_get_typedarray_type(value);
      switch (type) {
      case JERRY_TYPEDARRAY_UINT8:
        km_tty_printf("Uint8Array [");
        break;
      case JERRY_TYPEDARRAY_UINT8CLAMPED:
        km_tty_printf("Uint8ClampedArray [");
        break;
      case JERRY_TYPEDARRAY_INT8:
        km_tty_printf("Int8Array [");
        break;
      case JERRY_TYPEDARRAY_UINT16:
        km_tty_printf("Uint16Array [");
        break;
      case JERRY_TYPEDARRAY_INT16:
        km_tty_printf("Int16Array [");
        break;
      case JERRY_TYPEDARRAY_UINT32:
        km_tty_printf("Uint32Array [");
        break;
      case JERRY_TYPEDARRAY_INT32:
        km_tty_printf("Int32Array [");
        break;
      case JERRY_TYPEDARRAY_FLOAT32:
        km_tty_printf("Float32Array [");
        break;
      case JERRY_TYPEDARRAY_FLOAT64:
        km_tty_printf("Float64Array [");
        break;
      default:
        km_tty_printf("TypedArray [");
        break;
      }
      uint32_t len = jerry_get_typedarray_length (value);
      if (len > 0) {
        km_tty_printf("\r\n");
        for (int i = 0; i < len; i++) {
          jerry_value_t item = jerry_get_property_by_index(value, i);
          km_repl_pretty_print_indent(indent + 2);
          km_repl_pretty_print(indent + 2, depth - 1, item);
          if (i < len - 1) km_tty_printf(",");
          km_tty_printf("\r\n");
          jerry_release_value(item);
        }
        km_repl_pretty_print_indent(indent);
      }
      km_tty_printf("]");
    } else if (jerry_value_is_arraybuffer(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_boolean(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_constructor(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_dataview(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_error(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_function(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_number(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_null(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_promise(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_object(value)) {
      struct km_repl_pretty_print_object_foreach_data foreach_data = {indent, depth};
      jerry_foreach_object_property(value, km_repl_pretty_print_object_foreach_count, &foreach_data);
      km_tty_printf("{");
      if (foreach_data.count > 0) {
        km_tty_printf("\r\n");
        jerry_foreach_object_property(value, km_repl_pretty_print_object_foreach, &foreach_data);
        km_repl_pretty_print_indent(indent);
      }
      km_tty_printf("}");
    } else if (jerry_value_is_string(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_symbol(value)) {
      km_repl_pretty_print(indent, 0, value);
    } else if (jerry_value_is_undefined(value)) {
      km_repl_pretty_print(indent, 0, value);
    }
  }
}

void km_repl_println() {
  km_tty_printf("\r\n");
}

void km_repl_print_prompt() {
  km_tty_printf("\33[0m"); // back to normal color
  if (state.echo) {
    state.buffer[state.buffer_length] = '\0';
    km_tty_printf("\r\033[s"); // save cursor position
    km_tty_printf("> %s", &state.buffer);
    km_tty_printf("\33[H\33[900C\33[6n\033[u\033[2C"); // query terminal screen width and restore cursor position
  }
}

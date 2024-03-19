#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define EMPTY ""

#define MAX_NAME_LENGTH 2048

#define DEFAULT_IO_BUFFER_SIZE 8192

#define BYTES_PER_LINE 16
#define DEFAULT_LINE_INDENT 4
#define DEFAULT_ELEMENT_INDENT 1
#define DEFAULT_OUTPUT_FILENAME "./array.h"

#define VERSION "1.0"
#define VERSION_INFO "filetoarray version " VERSION " (build date: " __DATE__ " " __TIME__ ")\n"

#define HEADER_COMMENT "/*\n"                                                                                                     \
                       " * File: %s, size: %lu bytes.\n"                                                                          \
                       " *\n"                                                                                                     \
                       " * This code was generated by filetoarray tool (https://github.com/xreef/FileToArray).\n"                 \
                       " * Try filetoarray online: https://www.mischianti.org/online-converter-file-to-cpp-gzip-byte-array-3/.\n" \
                       " */\n\n"

#define ARRAY_SIZE_DEFINITION "#define %s_LEN %lu\n"
#define ARRAY_DECLARATION "%sconst unsigned char %s[%lu]%s"
#define ARRAY_LAST_MODIFIED_DEFINITION "#define %s_LAST_MODIFIED \"%s\"\n"

#define INLCUDE_HEADER_FILE "#include \"%s\"\n"
#define PROGMEM_IMPORT "#if defined ESP8266\n"   \
                        "#include <pgmspace.h>\n" \
                        "#endif\n\n"
#define INCLUDE_GUARD "#ifndef %s\n" \
                      "#define %s\n\n"
#define INCLUDE_GUARD_END "#endif /* %s */\n"

#define USAGE "Usage: filetoarray [options] file...\n"
#define HELP                                        \
  USAGE                                             \
  "Options:\n"                                      \
  "  -h           Display this information.\n"      \
  "  -i <width>   Set indentation width.\n"         \
  "  -o <file>    Place the output into <file>. \n" \
  "  -p           Use PROGMEM modifier.\n"          \
  "  -v           Display version information.\n"

enum type
{
  TYPE_DECLARATION = 1,
  TYPE_DEFINITION = 2
};

struct config_s
{
  enum
  {
    MODE_PROCESS,
    MODE_VERSION,
    MODE_HELP
  } mode;
  char *input_filename;
  char *output_filename;
  size_t indent;
  bool progmem;
} config_s_default = {MODE_PROCESS, NULL, DEFAULT_OUTPUT_FILENAME, DEFAULT_LINE_INDENT, false};

typedef struct config_s config;

const char *program_name;

void print_error_message(const char *error_message, ...)
{
  fprintf(stderr, "%s: ", program_name);
  va_list argps;
  va_start(argps, error_message);
  vfprintf(stderr, error_message, argps);
  va_end(argps);
}

char *get_basename(const char *path)
{
  char *base = NULL;
  for (char *cursor = (char *)path; *cursor != 0; cursor++)
  {
    if (*cursor == '/' || *cursor == '\\')
    {
      base = cursor;
    }
  }
  return base ? base + 1 : (char *)path;
}

long calculate_file_size(FILE *file)
{
  long current_position = ftell(file);
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fseek(file, current_position, SEEK_SET);
  return size;
}

char *get_varname_from(const char *filename, char *variable_name, size_t length)
{
  char *cursor = variable_name;
  for (char *name = get_basename(filename); *name != 0 && length > 0; cursor++, name++, length--)
  {
    *cursor = *name == '.' ? '_' : toupper(*name);
  }
  *cursor = 0;
  return variable_name;
}

char *transform_to_lowercase(char *string)
{
  for (int i = 0; string[i]; i++)
  {
    string[i] = tolower(string[i]);
  }
  return string;
}

void print_last_modified(FILE *output, const char *variable_name)
{
  time_t now = time(NULL);
  struct tm *current_time = gmtime(&now);
  char last_modified[30];
  strftime(last_modified, sizeof(last_modified), "%a, %d %b %Y %H:%M:%S GMT", current_time);
  fprintf(output, ARRAY_LAST_MODIFIED_DEFINITION "\n", variable_name, last_modified);
}

void print_content(FILE *input, FILE *output, const config *configuration)
{
  char buffer[DEFAULT_IO_BUFFER_SIZE];
  size_t bytes_read = 0, total_bytes = 0;
  while (bytes_read = fread(buffer, 1, sizeof(buffer), input))
  {
    for (long i = 0; i < bytes_read; i++, total_bytes++)
    {
      bool new_line = total_bytes % BYTES_PER_LINE == 0;
      int indent = new_line ? configuration->indent : DEFAULT_ELEMENT_INDENT;
      char *line_feed = (new_line && total_bytes) ? "\n" : EMPTY;
      char *separator = total_bytes ? "," : EMPTY;
      fprintf(output, "%s%s%*s0x%02X", separator, line_feed, indent, EMPTY, (uint8_t)buffer[i]);
    }
  }
}

void print_source_code(FILE *input, FILE *output, int output_type, const config *configuration)
{
  long file_size = calculate_file_size(input);
  fprintf(output, HEADER_COMMENT, get_basename(configuration->input_filename), file_size);
  char include_guard[MAX_NAME_LENGTH + 1];
  get_varname_from(configuration->output_filename, include_guard, MAX_NAME_LENGTH);
  if (output_type == TYPE_DECLARATION)
  {
    fprintf(output, INCLUDE_GUARD, include_guard, include_guard);
  }
  else if (output_type == TYPE_DEFINITION)
  {
    fprintf(output, INLCUDE_HEADER_FILE "\n", get_basename(configuration->output_filename));
  }
  char variable_name[MAX_NAME_LENGTH + 1];
  get_varname_from(configuration->input_filename, variable_name, MAX_NAME_LENGTH);
  if (output_type & TYPE_DECLARATION)
  {
    print_last_modified(output, variable_name);
    fprintf(output, ARRAY_SIZE_DEFINITION "\n", variable_name, file_size);
  }
  char *modifier = EMPTY;
  if (configuration->progmem && (output_type & TYPE_DEFINITION))
  {
    modifier = " PROGMEM";
    fprintf(output, PROGMEM_IMPORT);
  }
  char *storage_class = (output_type == TYPE_DEFINITION) ? "" : (output_type == TYPE_DECLARATION) ? "extern "
                                                                                                  : "static ";
  transform_to_lowercase(variable_name);
  fprintf(output, ARRAY_DECLARATION, storage_class, variable_name, file_size, modifier);
  if (output_type & TYPE_DEFINITION)
  {
    fprintf(output, " = {\n");
    print_content(input, output, configuration);
    fprintf(output, "}");
  }
  fprintf(output, ";\n");
  if (output_type == TYPE_DECLARATION)
  {
    fprintf(output, "\n" INCLUDE_GUARD_END, include_guard);
  }
}

bool is_header_file(const char *filename)
{
  char *dot = strrchr(filename, '.');
  return dot && (!strcmp(dot, ".h") || !strcmp(dot, ".hpp"));
}

char *convert_header_name_to_source_name(char *name)
{
  char *dot = strrchr(name, '.');
  if (*++dot == 'h')
  {
    *dot = 'c';
  }
  return name;
}

void process_file(const config *configuration)
{
  if (configuration->input_filename == NULL)
  {
    print_error_message("error: no input file\n" USAGE);
    exit(EXIT_FAILURE);
  }
  FILE *input_file = fopen(configuration->input_filename, "rb");
  if (input_file == NULL)
  {
    print_error_message("cannot find %s: %s\n", configuration->input_filename, strerror(errno));
    exit(EXIT_FAILURE);
  }
  char *output_filename = (configuration->output_filename) ? configuration->output_filename : DEFAULT_OUTPUT_FILENAME;
  FILE *output_file = fopen(output_filename, "wb");
  if (output_file == NULL)
  {
    fclose(input_file);
    print_error_message("cannot open output file %s: %s\n", output_filename, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (is_header_file(configuration->output_filename))
  {
    print_source_code(input_file, output_file, TYPE_DECLARATION, configuration);
    char *definition_output_filename = strdup(output_filename);
    definition_output_filename = convert_header_name_to_source_name(definition_output_filename);
    FILE *definition_output_file = fopen(definition_output_filename, "wb");
    if (definition_output_file == NULL)
    {
      fclose(input_file);
      fclose(output_file);
      print_error_message("cannot open output file %s: %s\n", definition_output_filename, strerror(errno));
      free(definition_output_filename);
      exit(EXIT_FAILURE);
    }
    print_source_code(input_file, definition_output_file, TYPE_DEFINITION, configuration);
    fclose(definition_output_file);
    free(definition_output_filename);
  }
  else
  {
    print_source_code(input_file, output_file, TYPE_DECLARATION | TYPE_DEFINITION, configuration);
  }
  fclose(input_file);
  fclose(output_file);
}

const config *parse_run_configuration(int argc, char *argv[], config *dest)
{
  size_t index;
  bool value = false;
  for (index = 1; index < argc && (*argv[index] == '-' || value); index++)
  {
    int arg_index = value ? (index - 1) : index;
    switch (argv[arg_index][1])
    {
    case 'o':
      if (value)
      {
        dest->output_filename = argv[index];
      }
      value = !value;
      break;
    case 'i':
      if (value)
      {
        dest->indent = atoi(argv[index]);
      }
      value = !value;
      break;
    case 'p':
      dest->progmem = true;
      break;
    case 'h':
      dest->mode = MODE_HELP;
      break;
    case 'v':
      dest->mode = MODE_VERSION;
      break;
    default:
      print_error_message("error: unrecognized command-line option '%s'\n", argv[index]);
      exit(EXIT_FAILURE);
    }
  }
  dest->input_filename = (index < argc) ? argv[index] : NULL;
}

void print_help()
{
  printf(HELP);
}

void print_version()
{
  printf(VERSION_INFO);
}

void configure_io_streams()
{
#ifdef _WIN32
  _setmode(0, _O_BINARY);
  _setmode(1, _O_BINARY);
#endif
}

int main(int argc, char *argv[])
{
  program_name = argv[0];
  configure_io_streams();
  config run_configuration = config_s_default;
  parse_run_configuration(argc, argv, &run_configuration);
  switch (run_configuration.mode)
  {
  case MODE_PROCESS:
    process_file(&run_configuration);
    break;
  case MODE_HELP:
    print_help();
    break;
  case MODE_VERSION:
    print_version();
    break;
  default:
    break;
  }
  return 0;
}
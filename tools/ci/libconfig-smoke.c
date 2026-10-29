/* Link against the actual vendored libconfig C sources, not a replica. */
#include "libconfig.h"
#include <stdio.h>
#include <string.h>

static int fail(const char *message) {
  fprintf(stderr, "libconfig smoke FAIL: %s\n", message);
  return 1;
}

int main(void) {
  config_t config;
  int value = 0;
  int enabled = 0;
  const char *name = NULL;
  config_init(&config);

  if (!config_read_string(&config,
      "value = 42;\n"
      "enabled = true;\n"
      "name = \"rAthena\";\n")) {
    config_destroy(&config);
    return fail("valid configuration rejected");
  }

  if (!config_lookup_int(&config, "value", &value) || value != 42) {
    config_destroy(&config);
    return fail("integer lookup");
  }
  if (!config_lookup_bool(&config, "enabled", &enabled) || enabled != 1) {
    config_destroy(&config);
    return fail("boolean lookup");
  }
  if (!config_lookup_string(&config, "name", &name) ||
      name == NULL || strcmp(name, "rAthena") != 0) {
    config_destroy(&config);
    return fail("string lookup");
  }

  config_destroy(&config);
  config_init(&config);
  if (config_read_string(&config, "value = ;\n")) {
    config_destroy(&config);
    return fail("invalid configuration accepted");
  }
  if (!config_error_text(&config) || !*config_error_text(&config)) {
    config_destroy(&config);
    return fail("parse error did not report diagnostics");
  }

  config_destroy(&config);
  puts("LIBCONFIG VENDOR SMOKE PASSED");
  return 0;
}

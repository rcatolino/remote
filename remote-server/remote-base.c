#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "dbusif.h"
#include "tcpserver.h"
#include "utils.h"

static char *opt_config_file = NULL;
static int opt_port          = DEFAULT_PORT;

static GOptionEntry opt_entries[] =
{
  { "config", 'c', 0, G_OPTION_ARG_FILENAME, &opt_config_file, "Config file to use. Default to ./remote-config.json", NULL },
  { "port", 'p', 0, G_OPTION_ARG_INT, &opt_port, "Tcp port to listen on. Default to 52000", NULL},
  { NULL}
};

void * processEvents(void * arg) {
  GMainLoop * loop = (GMainLoop *) arg;
  debug("starting event loop\n");
  g_main_loop_run(loop);
  debug("event loop stopped\n");
  return NULL;
}

int main(int argc, char *argv[]) {
  int socketd;
  char buff[MAX_CMD_SIZE+1];
  struct proxyParams * pp;

  GHashTable * call_table;
  GError * error;
  GMainLoop * loop;
  GOptionContext * opt_context;
  GThread * callback_thread;

  g_thread_init(NULL);
  g_type_init();
  loop = g_main_loop_new(NULL, FALSE);

  debug("Parsing options...\n");
  opt_context = g_option_context_new("remote-serv example");
  g_option_context_set_summary(opt_context,
                                "Example: to listen on port 42000 and read config from \'cfg.json\' use:\n"
                                "\n"
                                "  ./remote-serv -p 42000 -c cfg.json");
  g_option_context_add_main_entries(opt_context, opt_entries, NULL);
  error = NULL;
  if (!g_option_context_parse (opt_context, &argc, &argv, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    g_error_free(error);
    goto out;
  }

  if (opt_config_file == NULL) {
    opt_config_file = g_malloc(strlen(DEFAULT_CONFIG+1));
    strcpy(opt_config_file, DEFAULT_CONFIG);
  }

  debug("Command-line options parsed!\nParsing config file...\n");
  call_table = g_hash_table_new(g_str_hash, g_str_equal);
  if(loadConfig(opt_config_file) == -1) {
    // Bad config file, json syntax error.
    goto out;
  }

  if(parseConfig(&pp, call_table) == -1 || pp == NULL) {
    // Bad config file, proxy/method specifications error.
    goto out;
  }

  debug("Config file parsed!\nCreating proxies...\n");
  while (pp) {
    debug("Creating proxy for %s\n", pp->name);
    createConnection(pp, NULL);
    pp = pp->prev;
  }

  error = NULL;
  callback_thread = g_thread_create(processEvents, loop, FALSE, &error);
  if (!callback_thread) {
    debug("Error creating event loop thread : %s\n", error->message);
    g_error_free(error);
    goto out;
  }

  debug("Proxies created !\nCreating network connection...\n");
  socketd = initServer(opt_port);
  if (socketd == -1) {
    goto out;
  }

  debug("Connection created!\n");
  while (1) {
    struct callParams * cp = NULL;
    int clients = waitClient(socketd);
    if (clients == -1) {
      // We can't connect to a client.
      return -1;
    }

    int connected = 1;
    debug("connected to socket : %d\n", socketd);
    updateClientSocket(clients);
    updateMprisClientSocket(clients);
    while (connected) {
      int ret=receive(clients, buff, MAX_CMD_SIZE);
      transmit(clients, "\nPLAYING lo", 11);
      if (ret == -1) {
        closeClient(clients);
        connected = 0;
        continue;
      }

      buff[ret]='\0';
      debug("received : %s\n", buff);
      cp = g_hash_table_lookup(call_table, buff);
      if (cp) {
        debug("Found method %s() in %s associated with command %s. Calling...\n", cp->method, cp->proxy->name, buff);
        call(cp);
      }
    }
  }

out:
  g_option_context_free(opt_context);
  g_free(opt_config_file);

  return 0;
}

#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "config.h"
#include "dbusif.h"
#include "service-broadcast.h"
#include "streaming-server.h"
#include "tcpserver.h"
#include "utils.h"

static char *opt_config_file = NULL;
static int opt_port          = DEFAULT_PORT;
static int opt_daemon        = 0;

static GOptionEntry opt_entries[] =
{
  { "config", 'c', 0, G_OPTION_ARG_FILENAME, &opt_config_file,
    "Config file to use. Default to ./remote-config.json", NULL },
  { "port", 'p', 0, G_OPTION_ARG_INT, &opt_port, "Tcp port to listen on. Default to 52000", NULL},
  { "daemonize", 'd', 0, G_OPTION_ARG_NONE, &opt_daemon, "Launch process in backgrounf", NULL},
  { NULL}
};

void * processEvents(void * arg) {
  GMainLoop * loop = (GMainLoop *) arg;
  debug("starting event loop\n");
  g_main_loop_run(loop);
  debug("event loop stopped\n");
  return NULL;
}

void freeProfileRes(GHashTable *call_table, struct proxyParams *pp) {
  struct proxyParams *tmp;
  g_hash_table_remove_all(call_table);
  tmp = pp;
  while (tmp) {
    struct proxyParams *tmppp = tmp->prev;
    debug("Closing proxy for %s\n", tmp->name);
    closeConnection(tmp);
    tmp = tmppp;
  }
  deleteMprisInstance();
}

int createProfile(const char *profile, GHashTable *call_table, struct proxyParams **pp) {
  struct proxyParams *tmp;
  const char *profile_name = chooseProfile(profile);

  if(profile_name == NULL || parseConfig(pp, call_table) == -1 || *pp == NULL) {
    // Bad config file, proxy/method specifications error.
    debug("Config error!\n");
    return -1;
  }

  debug("\nConfig file parsed, for profile %s!\nCreating proxies...\n", profile_name);
  tmp = *pp;
  while (tmp) {
    debug("Creating proxy for %s\n", tmp->name);
    createConnection(tmp, NULL, NULL);
    tmp = tmp->prev;
  }

  return 0;
}

int initialize_options(int argc, char *argv[]) {
  GError * error = NULL;
  GOptionContext * opt_context;

  // Command-line options parsing.
  debug("Parsing options...\n");
  opt_context = g_option_context_new("");
  g_option_context_set_summary(opt_context,
                               "Example: to listen on port 42000 and read config from \'cfg.json\' use:\n"
                               "\n"
                               "  ./remote-server -p 42000 -c cfg.json");
  g_option_context_add_main_entries(opt_context, opt_entries, NULL);
  if (!g_option_context_parse(opt_context, &argc, &argv, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    g_error_free(error);
    g_option_context_free(opt_context);
    return -1;
  }

  // Options treatment.
  if (opt_daemon) {
    int ret = daemon(1, 0);
    if (ret == -1) {
      perror("Could not fork in background ");
    }
  }

  if (opt_config_file == NULL) {
    opt_config_file = g_malloc(strlen(DEFAULT_CONFIG+1));
    strcpy(opt_config_file, DEFAULT_CONFIG);
  }

  g_option_context_free(opt_context);
  return 0;
}

char *getargument(char *buff) {
  char *sep = strchr(buff, ' ');
  if (sep == NULL) {
    return NULL;
  }

  sep[0] = '\0';
  return sep+1;
}

int handle_command(int client_sock, char *buff, GHashTable *call_table, struct proxyParams *pp) {
  struct callParams *cp = NULL;
  char *argument = NULL;

  argument = getargument(buff);
  cp = g_hash_table_lookup(call_table, buff);
  if (cp) {
    debug("Found method %s() in %s associated with command %s. Calling...\n",
          cp->method, cp->proxy->name, buff);
    if (cp->custom_call) {
      cp->custom_call(cp, argument);
    } else {
      call(cp, NULL);
    }
  } else if (strlen(buff) >= POSITION_REQ_SZ &&
             strncmp(buff, POSITION_REQ, POSITION_REQ_SZ) == 0) {
    updatePositionProperty();
  } else if (strlen(buff) >= PROFILE_HEAD_SZ &&
             strncmp(buff, PROFILE_HEAD, PROFILE_HEAD_SZ) == 0) {
    debug("Profile change to %s.\n", argument);
    freeProfileRes(call_table, pp);
    createProfile(argument, call_table, &pp);
    updateMprisClientSocket(client_sock);
    sendCachedData();
  }
#ifdef AUDIO_FEEDBACK
  else if (strlen(buff) >= STREAMING_ON_REQ_SZ &&
             strncmp(buff, STREAMING_ON_REQ, STREAMING_ON_REQ_SZ) == 0) {
    startStreaming(loop, getClientAddress());
  } else if (strlen(buff) >= STREAMING_OFF_REQ_SZ &&
             strncmp(buff, STREAMING_OFF_REQ, STREAMING_OFF_REQ_SZ) == 0) {
    pauseStreaming();
  } else if (strlen(buff) >= STREAMING_STOP_SZ &&
             strncmp(buff, STREAMING_STOP, STREAMING_STOP_SZ) == 0) {
    deleteStreamingServer();
  }
#endif

  return 0;
}

int handle_client(int client_sock, GHashTable *call_table, struct proxyParams *pp) {
  char buff[MAX_CMD_SIZE+1];
  char *profiles;
  int connected = 1;

  updateClientSocket(client_sock);
  updateMprisClientSocket(client_sock);
  profiles = getProfiles();
  transmitMsg(client_sock, profiles, strlen(profiles),
              PROFILES_HEAD, PROFILES_HEAD_SZ);
  free(profiles);
  sendCachedData();
  while (connected) {
    int ret = receive(client_sock, buff, MAX_CMD_SIZE);
    if (ret == -1) {
      closeClient(client_sock);
      connected = 0;
      updateClientSocket(0);
      updateMprisClientSocket(0);
      continue;
    }

    buff[ret]='\0';
    handle_command(client_sock, buff, call_table, pp);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int tcpsocketd;
  struct proxyParams *pp;

  GHashTable *call_table;
  GError *error = NULL;
  GMainLoop *loop;
  GThread *callback_thread;
  GThread *svc_broadcast_thread;

  debug("uid : %d, gid : %d. %s %s\n", getuid(), getgid(), getenv("DISPLAY"), getenv("DBUS_SESSION_BUS_ADDRESS"));
  // Glib initialisation.
  // g_type_init();
  setlinebuf(stdout);
  if (initialize_options(argc, argv) == -1) {
    return -1;
  }

  debug("\nCommand-line options parsed!\nParsing config file...\n");
  // Glib event-loop creation. (For the dbus callbacks) (We need another thread
  // because we're not using glib for the networking.)
  loop = g_main_loop_new(NULL, FALSE);

  // Configuration file parsing/loading/treatment.
  // (This creates all the dbus interface)
  call_table = g_hash_table_new_full(g_str_hash, g_str_equal,
                                     (GDestroyNotify)free, (GDestroyNotify)free);
  if(loadConfig(opt_config_file) == -1) {
    // Bad config file, json syntax error.
    goto out;
  }

  if (createProfile(NULL, call_table, &pp) == -1) {
    goto out;
  }

  callback_thread = g_thread_try_new("loop", processEvents, loop, &error);
  if (!callback_thread) {
    debug("Error creating event loop thread : %s\n", error->message);
    goto error;
  }

  // Start tcp server
  debug("\nProxies created !\nCreating network connection...\n");
  tcpsocketd = initServer(opt_port);
  if (tcpsocketd == -1) {
    goto out;
  }

  {
    int port = opt_port;
    svc_broadcast_thread = g_thread_try_new("loop", serviceBroadcast, &port, &error);
  }
  if (!svc_broadcast_thread) {
    debug("Error creating service broadcast thread : %s\n", error->message);
    g_error_free(error);
  }

  debug("\nConnection created!\n");
  while (1) {
    int clients = waitClient(tcpsocketd);
    if (clients == -1) {
      // We can't connect to a client.
      goto out;
    }

    debug("\nconnected to socket : %d\n", tcpsocketd);
    handle_client(clients, call_table, pp);
  }

error:
  g_error_free(error);
out:
  g_free(opt_config_file);
#ifdef AUDIO_FEEDBACK
  deleteStreamingServer();
#endif
  g_main_loop_unref(loop);
  g_hash_table_unref(call_table);
  return 0;
}

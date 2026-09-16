#include <dbus/dbus.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;
static void on_signal(int sig) { (void)sig; running = 0; }

static DBusHandlerResult reply_empty(DBusConnection *c, DBusMessage *m) {
    DBusMessage *r = dbus_message_new_method_return(m);
    if (!r) return DBUS_HANDLER_RESULT_NEED_MEMORY;
    dbus_connection_send(c, r, NULL);
    dbus_message_unref(r);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult agent_handler(DBusConnection *c, DBusMessage *m, void *data) {
    const char *iface = dbus_message_get_interface(m);
    const char *member = dbus_message_get_member(m);
    (void)data;
    if (!member) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (iface && strcmp(iface, "org.freedesktop.DBus.Introspectable") == 0 && strcmp(member, "Introspect") == 0) {
        static const char xml[] =
          "<node><interface name='org.bluez.Agent1'>"
          "<method name='Release'/><method name='RequestPinCode'><arg type='o' direction='in'/><arg type='s' direction='out'/></method>"
          "<method name='DisplayPinCode'><arg type='o' direction='in'/><arg type='s' direction='in'/></method>"
          "<method name='RequestPasskey'><arg type='o' direction='in'/><arg type='u' direction='out'/></method>"
          "<method name='DisplayPasskey'><arg type='o' direction='in'/><arg type='u' direction='in'/><arg type='q' direction='in'/></method>"
          "<method name='RequestConfirmation'><arg type='o' direction='in'/><arg type='u' direction='in'/></method>"
          "<method name='RequestAuthorization'><arg type='o' direction='in'/></method>"
          "<method name='AuthorizeService'><arg type='o' direction='in'/><arg type='s' direction='in'/></method>"
          "<method name='Cancel'/></interface></node>";
        DBusMessage *r = dbus_message_new_method_return(m);
        const char *p = xml;
        if (!r) return DBUS_HANDLER_RESULT_NEED_MEMORY;
        dbus_message_append_args(r, DBUS_TYPE_STRING, &p, DBUS_TYPE_INVALID);
        dbus_connection_send(c, r, NULL); dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    if (!iface || strcmp(iface, "org.bluez.Agent1") != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp(member, "RequestPinCode") == 0) {
        const char *pin = "0000";
        DBusMessage *r = dbus_message_new_method_return(m);
        if (!r) return DBUS_HANDLER_RESULT_NEED_MEMORY;
        dbus_message_append_args(r, DBUS_TYPE_STRING, &pin, DBUS_TYPE_INVALID);
        dbus_connection_send(c, r, NULL); dbus_message_unref(r);
        fprintf(stdout, "agent: legacy PIN requested -> 0000\n"); fflush(stdout);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (strcmp(member, "RequestPasskey") == 0) {
        dbus_uint32_t passkey = 0;
        DBusMessage *r = dbus_message_new_method_return(m);
        if (!r) return DBUS_HANDLER_RESULT_NEED_MEMORY;
        dbus_message_append_args(r, DBUS_TYPE_UINT32, &passkey, DBUS_TYPE_INVALID);
        dbus_connection_send(c, r, NULL); dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (strcmp(member, "Release") == 0) {
        running = 0;
        return reply_empty(c,m);
    }
    if (!strcmp(member,"DisplayPinCode") || !strcmp(member,"DisplayPasskey") ||
        !strcmp(member,"RequestConfirmation") || !strcmp(member,"RequestAuthorization") ||
        !strcmp(member,"AuthorizeService") || !strcmp(member,"Cancel")) {
        fprintf(stdout, "agent: accepted %s\n", member); fflush(stdout);
        return reply_empty(c,m);
    }
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}


static int set_adapter_bool(DBusConnection *c, const char *prop, dbus_bool_t value) {
    DBusError e; DBusMessage *m,*r; DBusMessageIter it,var;
    const char *iface="org.bluez.Adapter1";
    dbus_error_init(&e);
    m=dbus_message_new_method_call("org.bluez","/org/bluez/hci0",
            "org.freedesktop.DBus.Properties","Set");
    if (!m) return -1;
    dbus_message_iter_init_append(m,&it);
    dbus_message_iter_append_basic(&it,DBUS_TYPE_STRING,&iface);
    dbus_message_iter_append_basic(&it,DBUS_TYPE_STRING,&prop);
    dbus_message_iter_open_container(&it,DBUS_TYPE_VARIANT,"b",&var);
    dbus_message_iter_append_basic(&var,DBUS_TYPE_BOOLEAN,&value);
    dbus_message_iter_close_container(&it,&var);
    r=dbus_connection_send_with_reply_and_block(c,m,5000,&e);
    dbus_message_unref(m);
    if (!r) { fprintf(stderr,"agent: set %s failed: %s\n",prop,e.message?e.message:""); dbus_error_free(&e); return -1; }
    dbus_message_unref(r); return 0;
}

static int set_adapter_string(DBusConnection *c, const char *prop, const char *value) {
    DBusError e; DBusMessage *m,*r; DBusMessageIter it,var;
    const char *iface="org.bluez.Adapter1";
    dbus_error_init(&e);
    m=dbus_message_new_method_call("org.bluez","/org/bluez/hci0",
            "org.freedesktop.DBus.Properties","Set");
    if (!m) return -1;
    dbus_message_iter_init_append(m,&it);
    dbus_message_iter_append_basic(&it,DBUS_TYPE_STRING,&iface);
    dbus_message_iter_append_basic(&it,DBUS_TYPE_STRING,&prop);
    dbus_message_iter_open_container(&it,DBUS_TYPE_VARIANT,"s",&var);
    dbus_message_iter_append_basic(&var,DBUS_TYPE_STRING,&value);
    dbus_message_iter_close_container(&it,&var);
    r=dbus_connection_send_with_reply_and_block(c,m,5000,&e);
    dbus_message_unref(m);
    if (!r) { fprintf(stderr,"agent: set %s failed: %s\n",prop,e.message?e.message:""); dbus_error_free(&e); return -1; }
    dbus_message_unref(r); return 0;
}

static int call_agent_manager(DBusConnection *c, const char *method, int with_cap) {
    DBusError e; DBusMessage *m,*r;
    const char *path="/com/tomtom/agent", *cap="NoInputNoOutput";
    dbus_error_init(&e);
    m=dbus_message_new_method_call("org.bluez","/org/bluez","org.bluez.AgentManager1",method);
    if (!m) return -1;
    if (with_cap) dbus_message_append_args(m,DBUS_TYPE_OBJECT_PATH,&path,DBUS_TYPE_STRING,&cap,DBUS_TYPE_INVALID);
    else dbus_message_append_args(m,DBUS_TYPE_OBJECT_PATH,&path,DBUS_TYPE_INVALID);
    r=dbus_connection_send_with_reply_and_block(c,m,10000,&e);
    dbus_message_unref(m);
    if (!r) {
        fprintf(stderr,"agent: %s failed: %s %s\n",method,e.name?e.name:"",e.message?e.message:"");
        dbus_error_free(&e); return -1;
    }
    dbus_message_unref(r); return 0;
}


static int wait_register_agent(DBusConnection *c) {
    int i;
    for (i = 1; i <= 30 && running; i++) {
        if (call_agent_manager(c, "RegisterAgent", 1) == 0) {
            fprintf(stdout, "agent: RegisterAgent succeeded on attempt %d\n", i);
            return 0;
        }
        fprintf(stderr, "agent: AgentManager1 not ready, retry %d/30\n", i);
        sleep(1);
    }
    return -1;
}

static int wait_default_agent(DBusConnection *c) {
    int i;
    for (i = 1; i <= 15 && running; i++) {
        if (call_agent_manager(c, "RequestDefaultAgent", 0) == 0) {
            fprintf(stdout, "agent: RequestDefaultAgent succeeded on attempt %d\n", i);
            return 0;
        }
        fprintf(stderr, "agent: default-agent request retry %d/15\n", i);
        sleep(1);
    }
    return -1;
}

static void wait_configure_adapter(DBusConnection *c) {
    int i;
    for (i = 1; i <= 30 && running; i++) {
        if (set_adapter_bool(c, "Powered", 1) == 0) {
            set_adapter_bool(c, "Pairable", 1);
            set_adapter_bool(c, "Discoverable", 1);
            set_adapter_string(c, "Alias", "TomTom ONE XL");
            fprintf(stdout, "agent: hci0 configured on attempt %d\n", i);
            return;
        }
        fprintf(stderr, "agent: hci0 not ready, retry %d/30\n", i);
        sleep(1);
    }
    fprintf(stderr, "agent: WARNING: hci0 properties not configured\n");
}

int main(void) {
    DBusError e; DBusConnection *c;
    static const DBusObjectPathVTable vt={ .message_function=agent_handler };
    signal(SIGTERM,on_signal); signal(SIGINT,on_signal); signal(SIGPIPE,SIG_IGN);
    setvbuf(stdout,NULL,_IOLBF,0); setvbuf(stderr,NULL,_IOLBF,0);
    dbus_error_init(&e);
    c=dbus_bus_get(DBUS_BUS_SYSTEM,&e);
    if (!c) { fprintf(stderr,"agent: system bus: %s\n",e.message?e.message:""); return 1; }
    if (!dbus_connection_register_object_path(c,"/com/tomtom/agent",&vt,NULL)) return 2;
    if (wait_register_agent(c) < 0) return 3;
    if (wait_default_agent(c) < 0) return 4;
    wait_configure_adapter(c);
    puts("agent: default NoInputNoOutput agent registered; discoverable/pairable; legacy PIN 0000");
    while (running && dbus_connection_get_is_connected(c))
        dbus_connection_read_write_dispatch(c,1000);
    dbus_connection_unregister_object_path(c,"/com/tomtom/agent");
    dbus_connection_unref(c);
    return 0;
}

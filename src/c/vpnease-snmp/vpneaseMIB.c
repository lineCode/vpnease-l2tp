/*
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.scalar.conf,v 1.9 2005/01/07 09:37:18 dts12 Exp $
 */

/* conditional to make testing easier */
#ifndef WITHOUT_NETSNMP_INTERFACE
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include "vpneaseMIB.h"
#endif /* #ifndef WITHOUT_NETSNMP_INTERFACE */

/* ---------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/stat.h>

#define DATA_FILE_READ_TIME_LIMIT (15*60)

struct key_value_node {
  struct key_value_node *next;
  char *key;
  char *value;
};

#ifdef TESTER_DEBUG
static char *data_file_name = "/tmp/snmp-data.txt";
#else
static char *data_file_name = "/var/run/l2tpgw/snmp-data.txt";
#endif
static time_t data_file_last_read = -1;
static time_t data_file_last_mtime = -1;
static struct key_value_node *data_file_keyvalue_list = NULL;

void data_file_free_keyvalue_list(void) {
  struct key_value_node *p, *q;

  if (data_file_keyvalue_list == NULL) {
    return;
  }

  p = data_file_keyvalue_list;
  while (p) {
    q = p;
    p = p->next;
    if (q->key) {
      free(q->key);
    }
    if (q->value) {
      free(q->value);
    }
    free(q);
  }

  data_file_keyvalue_list = NULL;
}

void data_file_parse_keyvalues(FILE *f) {
  int state = 0;  /* 0 = parsing key, 1 = parsing value */
  char keybuf[1024];
  int keyoff = 0;
  char valbuf[1024];
  int valoff = 0;

  for(;;) {
    int ch = fgetc(f);

    /* safety check */
    if ((keyoff >= sizeof(keybuf)) || (valoff >= sizeof(valbuf))) {
      /* cannot take any key or val characters anymore .. just bail out */
      return;
    }

    if (state == 0) {
      if (ch == EOF) {
	/* unexpected, ignore last key-value */
	;
      } else if (ch == 0x0a) {
	/* unexpected, ignore line */
	state = 0;
	keyoff = 0;
	valoff = 0;
	continue;
      } else if (ch == '=') {
	state =1;
	keybuf[keyoff++] = (char) 0x00;
	valoff = 0;
      } else {
	keybuf[keyoff++] = (char) ch;
      }
    } else if (state == 1) {
      if ((ch == EOF) || (ch == 0x0a)) {
	/* completed key-value pair */
	struct key_value_node *n = NULL;

	valbuf[valoff++] = (char) 0x00;

	/* sanity */
	if ((keyoff <= 0) || (valoff <= 0)) {
	  state = 0;
	  keyoff = 0;
	  valoff = 0;
	  continue;
	}

	n = malloc(sizeof(struct key_value_node));
	if (!n) {
	  /* out of memory, bail out */
	  return;
	}
	memset(n, 0x00, sizeof(*n));

	/* if malloc succeeds, the node is always added; the key/value may be bogus */
	n->key = malloc(keyoff);
	n->value = malloc(valoff);
	if (n->key) {
	  strncpy(n->key, keybuf, keyoff);
	  n->key[keyoff - 1] = 0x00;
	}
	if (n->value) {
	  strncpy(n->value, valbuf, valoff);
	  n->value[valoff - 1] = 0x00;
	}
	n->next = data_file_keyvalue_list;
	data_file_keyvalue_list = n;

	state = 0;
	keyoff = 0;
	valoff = 0;
      } else {
	valbuf[valoff++] = (char) ch;
      }
    } else {
      /* should not get here */
      return;
    }

    if (ch == EOF) {
      break;
    }
  }
}

int data_file_refresh(void) {
  time_t now = time(NULL);
  int update_now = 0;
  FILE *f;

  /*
   *  Various conditions for re-reading the SNMP data file
   */
  if ((data_file_last_read == -1) || (data_file_last_mtime == -1)) {
    /* uninitialized, read always */
#ifdef TESTER_DEBUG
    printf("uninitialized -> update_now\n");
#endif
    update_now = 1;
  } else {
    /* if data too old or time diff insane, read */
    int diff_read = now - data_file_last_read;
    if ((diff_read < 0) || (diff_read >= DATA_FILE_READ_TIME_LIMIT)) {
#ifdef TESTER_DEBUG
      printf("too old -> update_now\n");
#endif
      update_now = 1;
    } else {
      /* if file modified, read; most expensive check */
      struct stat mystat;
      memset(&mystat, 0x00, sizeof(mystat));
      if (lstat(data_file_name, &mystat) != 0) {
	/* lstat failed, re-read */
#ifdef TESTER_DEBUG
	printf("lstat failed -> update_now\n");
#endif
	update_now = 1;
      } else {
	if (mystat.st_mtime != data_file_last_mtime) {
#ifdef TESTER_DEBUG
	  printf("lstat mtime -> update_now\n");
#endif
	  update_now = 1;
	}
      }
    }
  }

  /*
   *  If no update, bail out now
   */
  if (!update_now) {
    return 0;
  }

#ifdef TESTER_DEBUG
  printf("re-reading data file\n");
#endif

  /*
   *  Re-read key-value pairs
   */
  data_file_free_keyvalue_list();

  f = fopen(data_file_name, "rb");
  if (!f) {
    return -1;
  }
  data_file_parse_keyvalues(f);
  if (f) {
    fclose(f);
  }

  /* update timestamps */
  if (1) {
    struct stat mystat;

    data_file_last_read = now;

    memset(&mystat, 0x00, sizeof(mystat));
    if (lstat(data_file_name, &mystat) == 0) {
      data_file_last_mtime = mystat.st_mtime;
    } else {
      /* not very nice, but what to do? */
      ;
    }
  }

  return 0;
}

struct key_value_node *data_file_get_keyvalue(char *key) {
  struct key_value_node *p;

  data_file_refresh();

  if (data_file_keyvalue_list == NULL) {
    return NULL;
  }

  p = data_file_keyvalue_list;
  while (p) {
    if (p->key != NULL && p-> value != NULL) {  /* check both to avoid half-broken keyvalues caused by OOM */
      if (strcmp(p->key, key) == 0) {
	return p;
      }
    }
    p = p->next;
  }

  return NULL;
}

int data_file_get_int(char *key, int defvalue) {
  struct key_value_node *kv = data_file_get_keyvalue(key);
  if (kv) {
    return atoi(kv->value);
  } else {
    return defvalue;
  }
}

char *data_file_get_string(char *key, char *defvalue) {
  struct key_value_node *kv = data_file_get_keyvalue(key);
  if (kv) {
    return kv->value;
  } else {
    return defvalue;
  }
}

int data_file_get_ip_address(char *key, int defvalue) {
  struct key_value_node *kv = data_file_get_keyvalue(key);
  if (kv) {
    int a, b, c, d;
    int t;

    if (sscanf(kv->value, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
      return htonl(defvalue);
    }

    t = (a << 24) | (b << 16) | (c << 8) | d;
    return htonl(t);
  } else {
    return htonl(defvalue);
  }
}

/* conditional to make testing easier */
#ifndef WITHOUT_NETSNMP_INTERFACE
/* ---------------------------------------------------------------------- */

/** Initializes the vpneaseMIB module */
void
init_vpneaseMIB(void)
{
    static oid vpneaseHealthCheckErrors_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,102 };
    static oid vpneaseUserCount_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,103 };
    static oid vpneaseSiteToSiteCount_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,104 };
    static oid vpneaseServiceUptime_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,105 };
    static oid vpneaseLastMaintenanceReboot_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,106 };
    static oid vpneaseNextMaintenanceReboot_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,107 };
    static oid vpneaseLastSoftwareUpdate_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,108 };
    static oid vpneaseSoftwareVersion_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,109 };
    static oid vpneaseCpuUsage_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,201 };
    static oid vpneaseMemoryUsage_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,202 };
    static oid vpneaseVirtualMemoryUsage_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,203 };
    static oid vpneaseHostUptime_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,204 };
    static oid vpneasePublicAddress_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,301 };
    static oid vpneasePublicSubnet_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,302 };
    static oid vpneasePublicMac_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,303 };
    static oid vpneasePrivateAddress_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,304 };
    static oid vpneasePrivateSubnet_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,305 };
    static oid vpneasePrivateMac_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,306 };
    static oid vpneaseLicenseKey_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,401 };
    static oid vpneaseLicenseString_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,402 };
    static oid vpneaseLicenseUserLimit_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,403 };
    static oid vpneaseLicenseSiteToSiteLimit_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,404 };
    static oid vpneaseMaintenanceReboots_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,405 };
    static oid vpneaseWatchdogReboots_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,406 };
    static oid vpneaseLicenseServerConnection_oid[] = { 1,3,6,1,4,1,26058,1,1,1,1,407 };

  DEBUGMSGTL(("vpneaseMIB", "Initializing\n"));

    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseHealthCheckErrors", handle_vpneaseHealthCheckErrors,
                               vpneaseHealthCheckErrors_oid, OID_LENGTH(vpneaseHealthCheckErrors_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseUserCount", handle_vpneaseUserCount,
                               vpneaseUserCount_oid, OID_LENGTH(vpneaseUserCount_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseSiteToSiteCount", handle_vpneaseSiteToSiteCount,
                               vpneaseSiteToSiteCount_oid, OID_LENGTH(vpneaseSiteToSiteCount_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseServiceUptime", handle_vpneaseServiceUptime,
                               vpneaseServiceUptime_oid, OID_LENGTH(vpneaseServiceUptime_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLastMaintenanceReboot", handle_vpneaseLastMaintenanceReboot,
                               vpneaseLastMaintenanceReboot_oid, OID_LENGTH(vpneaseLastMaintenanceReboot_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseNextMaintenanceReboot", handle_vpneaseNextMaintenanceReboot,
                               vpneaseNextMaintenanceReboot_oid, OID_LENGTH(vpneaseNextMaintenanceReboot_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLastSoftwareUpdate", handle_vpneaseLastSoftwareUpdate,
                               vpneaseLastSoftwareUpdate_oid, OID_LENGTH(vpneaseLastSoftwareUpdate_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseSoftwareVersion", handle_vpneaseSoftwareVersion,
                               vpneaseSoftwareVersion_oid, OID_LENGTH(vpneaseSoftwareVersion_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseCpuUsage", handle_vpneaseCpuUsage,
                               vpneaseCpuUsage_oid, OID_LENGTH(vpneaseCpuUsage_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseMemoryUsage", handle_vpneaseMemoryUsage,
                               vpneaseMemoryUsage_oid, OID_LENGTH(vpneaseMemoryUsage_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseVirtualMemoryUsage", handle_vpneaseVirtualMemoryUsage,
                               vpneaseVirtualMemoryUsage_oid, OID_LENGTH(vpneaseVirtualMemoryUsage_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseHostUptime", handle_vpneaseHostUptime,
                               vpneaseHostUptime_oid, OID_LENGTH(vpneaseHostUptime_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneasePublicAddress", handle_vpneasePublicAddress,
                               vpneasePublicAddress_oid, OID_LENGTH(vpneasePublicAddress_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneasePublicSubnet", handle_vpneasePublicSubnet,
                               vpneasePublicSubnet_oid, OID_LENGTH(vpneasePublicSubnet_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneasePublicMac", handle_vpneasePublicMac,
                               vpneasePublicMac_oid, OID_LENGTH(vpneasePublicMac_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneasePrivateAddress", handle_vpneasePrivateAddress,
                               vpneasePrivateAddress_oid, OID_LENGTH(vpneasePrivateAddress_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneasePrivateSubnet", handle_vpneasePrivateSubnet,
                               vpneasePrivateSubnet_oid, OID_LENGTH(vpneasePrivateSubnet_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneasePrivateMac", handle_vpneasePrivateMac,
                               vpneasePrivateMac_oid, OID_LENGTH(vpneasePrivateMac_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLicenseKey", handle_vpneaseLicenseKey,
                               vpneaseLicenseKey_oid, OID_LENGTH(vpneaseLicenseKey_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLicenseString", handle_vpneaseLicenseString,
                               vpneaseLicenseString_oid, OID_LENGTH(vpneaseLicenseString_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLicenseUserLimit", handle_vpneaseLicenseUserLimit,
                               vpneaseLicenseUserLimit_oid, OID_LENGTH(vpneaseLicenseUserLimit_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLicenseSiteToSiteLimit", handle_vpneaseLicenseSiteToSiteLimit,
                               vpneaseLicenseSiteToSiteLimit_oid, OID_LENGTH(vpneaseLicenseSiteToSiteLimit_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseMaintenanceReboots", handle_vpneaseMaintenanceReboots,
                               vpneaseMaintenanceReboots_oid, OID_LENGTH(vpneaseMaintenanceReboots_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseWatchdogReboots", handle_vpneaseWatchdogReboots,
                               vpneaseWatchdogReboots_oid, OID_LENGTH(vpneaseWatchdogReboots_oid),
                               HANDLER_CAN_RONLY
        ));
    netsnmp_register_scalar(
        netsnmp_create_handler_registration("vpneaseLicenseServerConnection", handle_vpneaseLicenseServerConnection,
                               vpneaseLicenseServerConnection_oid, OID_LENGTH(vpneaseLicenseServerConnection_oid),
                               HANDLER_CAN_RONLY
        ));
}

static int handle_octet_string(netsnmp_mib_handler *handler,
			       netsnmp_handler_registration *reginfo,
			       netsnmp_agent_request_info   *reqinfo,
			       netsnmp_request_info         *requests,
			       char *key) {
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    char *value = data_file_get_string(key, "");
    
    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_value(requests->requestvb, ASN_OCTET_STR,
                                     (u_char *) value,
                                     strlen(value));
            break;


        default:
	    /* we should never get here, so this is a really bad error */
	    snmp_log(LOG_ERR, "unknown mode (%d), key %s\n", reqinfo->mode, key );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

/* ---------------------------------------------------------------------- */

static int handle_int_based(netsnmp_mib_handler *handler,
			    netsnmp_handler_registration *reginfo,
			    netsnmp_agent_request_info   *reqinfo,
			    netsnmp_request_info         *requests,
			    char *key,
			    int asn_type) {
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */
    
    int value = data_file_get_int(key, 0);

    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_value(requests->requestvb, asn_type,
                                     (u_char *) &value,
                                     sizeof(value));
            break;


        default:
            /* we should never get here, so this is a really bad error */
	    snmp_log(LOG_ERR, "unknown mode (%d), key %s\n", reqinfo->mode, key );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

static int handle_gauge(netsnmp_mib_handler *handler,
			netsnmp_handler_registration *reginfo,
			netsnmp_agent_request_info   *reqinfo,
			netsnmp_request_info         *requests,
			char *key) {
    return handle_int_based(handler, reginfo, reqinfo, requests, key, ASN_GAUGE);
}

static int handle_timeticks(netsnmp_mib_handler *handler,
			    netsnmp_handler_registration *reginfo,
			    netsnmp_agent_request_info   *reqinfo,
			    netsnmp_request_info         *requests,
			    char *key) {
    return handle_int_based(handler, reginfo, reqinfo, requests, key, ASN_TIMETICKS);
}

static int handle_counter(netsnmp_mib_handler *handler,
			  netsnmp_handler_registration *reginfo,
			  netsnmp_agent_request_info   *reqinfo,
			  netsnmp_request_info         *requests,
			  char *key) {
    return handle_int_based(handler, reginfo, reqinfo, requests, key, ASN_COUNTER);
}

static int handle_integer(netsnmp_mib_handler *handler,
			  netsnmp_handler_registration *reginfo,
			  netsnmp_agent_request_info   *reqinfo,
			  netsnmp_request_info         *requests,
			  char *key) {
    return handle_int_based(handler, reginfo, reqinfo, requests, key, ASN_INTEGER);
}

static int handle_ipaddress(netsnmp_mib_handler *handler,
			    netsnmp_handler_registration *reginfo,
			    netsnmp_agent_request_info   *reqinfo,
			    netsnmp_request_info         *requests,
			    char *key) {
    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    int value = data_file_get_ip_address(key, 0x00000000);
    
    switch(reqinfo->mode) {

        case MODE_GET:
            snmp_set_var_typed_value(requests->requestvb, ASN_IPADDRESS,
                                     (u_char *) &value,
                                     sizeof(value));
            break;


        default:
            /* we should never get here, so this is a really bad error */
	    snmp_log(LOG_ERR, "unknown mode (%d), key %s\n", reqinfo->mode, key );
            return SNMP_ERR_GENERR;
    }

    return SNMP_ERR_NOERROR;
}

/* ---------------------------------------------------------------------- */

int
handle_vpneaseHealthCheckErrors(netsnmp_mib_handler *handler,
				netsnmp_handler_registration *reginfo,
				netsnmp_agent_request_info   *reqinfo,
				netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseHealthCheckErrors");
}
int
handle_vpneaseUserCount(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseUserCount");
}
int
handle_vpneaseSiteToSiteCount(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseSiteToSiteCount");
}

int
handle_vpneaseServiceUptime(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_timeticks(handler, reginfo, reqinfo, requests, "vpneaseServiceUptime");
}

int
handle_vpneaseLastMaintenanceReboot(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneaseLastMaintenanceReboot");
}

int
handle_vpneaseNextMaintenanceReboot(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneaseNextMaintenanceReboot");
}

int
handle_vpneaseLastSoftwareUpdate(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneaseLastSoftwareUpdate");
}

int
handle_vpneaseSoftwareVersion(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneaseSoftwareVersion");
}

int
handle_vpneaseCpuUsage(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseCpuUsage");
}

int
handle_vpneaseMemoryUsage(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseMemoryUsage");
}

int
handle_vpneaseVirtualMemoryUsage(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseVirtualMemoryUsage");
}

int
handle_vpneaseHostUptime(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_timeticks(handler, reginfo, reqinfo, requests, "vpneaseHostUptime");
}

int
handle_vpneasePublicAddress(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_ipaddress(handler, reginfo, reqinfo, requests, "vpneasePublicAddress");
}

int
handle_vpneasePublicSubnet(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_ipaddress(handler, reginfo, reqinfo, requests, "vpneasePublicSubnet");
}

int
handle_vpneasePublicMac(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneasePublicMac");
}

int
handle_vpneasePrivateAddress(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_ipaddress(handler, reginfo, reqinfo, requests, "vpneasePrivateAddress");
}

int
handle_vpneasePrivateSubnet(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_ipaddress(handler, reginfo, reqinfo, requests, "vpneasePrivateSubnet");
}

int
handle_vpneasePrivateMac(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneasePrivateMac");
}

int
handle_vpneaseLicenseKey(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneaseLicenseKey");
}

int
handle_vpneaseLicenseString(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_octet_string(handler, reginfo, reqinfo, requests, "vpneaseLicenseString");
}

int
handle_vpneaseLicenseUserLimit(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseLicenseUserLimit");
}

int
handle_vpneaseLicenseSiteToSiteLimit(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_gauge(handler, reginfo, reqinfo, requests, "vpneaseLicenseSiteToSiteLimit");
}

int
handle_vpneaseMaintenanceReboots(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_counter(handler, reginfo, reqinfo, requests, "vpneaseMaintenanceReboots");
}

int
handle_vpneaseWatchdogReboots(netsnmp_mib_handler *handler,
                          netsnmp_handler_registration *reginfo,
                          netsnmp_agent_request_info   *reqinfo,
                          netsnmp_request_info         *requests)
{
    return handle_counter(handler, reginfo, reqinfo, requests, "vpneaseWatchdogReboots");
}

int
handle_vpneaseLicenseServerConnection(netsnmp_mib_handler *handler,
				      netsnmp_handler_registration *reginfo,
				      netsnmp_agent_request_info   *reqinfo,
				      netsnmp_request_info         *requests)
{
    return handle_integer(handler, reginfo, reqinfo, requests, "vpneaseLicenseServerConnection");
}

#endif /* #ifndef WITHOUT_NETSNMP_INTERFACE */

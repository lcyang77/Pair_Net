#include "cc_hal_ble.h"

#include "cc_log.h"

#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

static char *TAG = "cc_hal_ble";

CC_EVENT_DEFINE_BASE(CC_HAL_BLE_EVENT);

/* 59462f12-9543-9999-12c8-58b459a2712d */
static const ble_uuid16_t gatt_svr_svc_sec_test_uuid =
    BLE_UUID16_INIT(0xFFE5);

/* 659e-897e-45e1-b016-007107c96df6 */
static const ble_uuid16_t gatt_svr_chr_sec_write_uuid =
        BLE_UUID16_INIT(0xFFF3);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */
static const ble_uuid16_t gatt_svr_chr_sec_read_uuid =
        BLE_UUID16_INIT(0xFFF4);


static uint8_t g_ble_is_init = 0;
static cc_hal_ble_recv_cb_t g_ble_recv_cb = NULL;

static uint16_t g_ble_conn_handle = 0;

static int gap_event(struct ble_gap_event *event, void *arg);
static int gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg);

uint16_t csc_notify_handle;
uint16_t csc_write_handle;
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_sec_test_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /*** Characteristic: Random number generator. */
            .uuid = &gatt_svr_chr_sec_write_uuid.u,
            .access_cb = gatt_svr_chr_access_sec_test,
            .val_handle = &csc_write_handle,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            /*** Characteristic: Static value. */
            .uuid = &gatt_svr_chr_sec_read_uuid.u,
            .access_cb = gatt_svr_chr_access_sec_test,
            .val_handle = &csc_notify_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            0, /* No more characteristics in this service. */
        } },
    },

    {
        0, /* No more services. */
    },
};

static void __port_task(void *param)
{
    nimble_port_run();

    nimble_port_freertos_deinit();
}
static void on_reset(int reason){

}

static void on_sync(void)
{
    int rc;

    static uint8_t ble_addr_type;

    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(ble_addr_type, addr_val, NULL);
    
    cc_event_post(CC_HAL_BLE_EVENT, CC_HAL_BLE_EVENT_ENABLED, NULL, 0);

    g_ble_is_init = 1;
}

static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}
static int gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    int rc;
    uint16_t read_len = 0;
    const ble_uuid_t *uuid;
    static uint8_t gatt_svr_sec_recv_buf[20];

    uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_write_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    1,
                                    sizeof(gatt_svr_sec_recv_buf),
                                    gatt_svr_sec_recv_buf, &read_len);

            CC_LOGD(TAG, "gatt_svr_chr_write: %d", read_len);
            if (g_ble_recv_cb && read_len) {
                g_ble_recv_cb(gatt_svr_sec_recv_buf, read_len);
            }
            return rc;
        default:
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static void __start_adv(void){
    int rc;
    uint8_t own_addr_type;
    struct ble_gap_adv_params adv_params;

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        CC_LOGE(TAG, "rc=%x", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    adv_params.itvl_min = 120;
    adv_params.itvl_max = 120;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event, NULL);
    if (rc != 0) {
        CC_LOGE(TAG, "rc=%x", rc);
        return;
    }
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    int rc;
    struct ble_gap_conn_desc desc;

    CC_LOGD(TAG, "event->type: %d", event->type);

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
        }

        g_ble_conn_handle = event->connect.conn_handle;
		cc_event_post(CC_HAL_BLE_EVENT, CC_HAL_BLE_EVENT_CONNECTED, NULL, 0);
	
        if (event->connect.status != 0) {
            /* Connection failed; resume advertising. */
            __start_adv();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
		cc_event_post(CC_HAL_BLE_EVENT, CC_HAL_BLE_EVENT_DISCONNECTED, NULL, 0);
        __start_adv();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        assert(rc == 0);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);

        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    return 0;
}

cc_err_t cc_hal_ble_start_advzertising(uint8_t *adv_data, uint8_t adv_len, uint8_t *scan_rsp_data, uint8_t scan_rsp_len){
    int rc;

    rc = ble_gap_adv_set_data(adv_data, adv_len);
    if(rc != 0){
        CC_LOGE(TAG, "rc=%x", rc);
        return CC_FAIL;
    }

    rc = ble_gap_adv_rsp_set_data(scan_rsp_data, scan_rsp_len);
    if(rc != 0){
        CC_LOGE(TAG, "rc=%x", rc);
        return CC_FAIL;
    }

    __start_adv();

    return CC_OK;
}

cc_err_t cc_hal_ble_stop_advzertising(void){
    return CC_ERR_NOT_SUPPORTED;
}

cc_err_t cc_hal_ble_reset_advzertising(uint8_t *adv_data, uint8_t adv_len, uint8_t *scan_rsp_data, uint8_t scan_rsp_len){
    return CC_ERR_NOT_SUPPORTED;
}

uint16_t cc_hal_ble_send(uint8_t *data, uint16_t len){

    struct os_mbuf *om;
    if (0 == len || NULL == data) {
        return CC_ERR_INVALID_ARG;
    }

    for (size_t idx = 0; idx < len; idx += 20){
        om = ble_hs_mbuf_from_flat(data + idx, ((len - idx)>20)?20:((len - idx)));
	    ble_gattc_notify_custom(g_ble_conn_handle, csc_notify_handle, om);
    }
    return 0;
}

cc_err_t cc_hal_ble_get_mac(uint8_t *mac){
    return CC_ERR_NOT_SUPPORTED;
}

cc_err_t cc_hal_ble_disconnect(void){
    return CC_ERR_NOT_SUPPORTED;
}

cc_err_t cc_hal_ble_init(cc_hal_ble_recv_cb_t recv_cb){
    int rc;

    nimble_port_init();

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return CC_FAIL;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return CC_FAIL;
    }
    
    rc = ble_svc_gap_device_name_set("CC");
    if (rc != 0) {
        return CC_FAIL;
    }

    nimble_port_freertos_init(__port_task);

    g_ble_recv_cb = recv_cb;

    return CC_OK;
}

cc_err_t cc_hal_ble_deinit(void){
    nimble_port_stop();

    nimble_port_deinit();

    g_ble_recv_cb = NULL;

	g_ble_is_init = 0;

    return CC_OK;
}
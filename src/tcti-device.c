#include "tcti-device.h"

/* Boiler-plate gobject code. */
static gpointer tcti_device_parent_class = NULL;

enum {
    PROP_0,
    PROP_FILENAME,
    N_PROPERTIES
};
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

static void
tcti_device_set_property (GObject      *object,
                          guint         property_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    TctiDevice *self = TCTI_DEVICE (object);

    switch (property_id) {
    case PROP_FILENAME:
        g_free (self->filename);
        self->filename = g_value_dup_string (value);
        g_debug ("TctiDevice set filename: %s", self->filename);
        break;
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}
static void
tcti_device_get_property (GObject    *object,
                          guint       property_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    TctiDevice *self = TCTI_DEVICE (object);

    switch (property_id) {
    case PROP_FILENAME:
        g_value_set_string (value, self->filename);
        break;
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}
/* override the parent finalize method so we can free the data associated with
 * the TctiDevice instance.
 */
static void
tcti_device_finalize (GObject *obj)
{
    TctiDevice *tcti_device = TCTI_DEVICE (obj);

    g_free (tcti_device->filename);
    if (tcti_device_parent_class)
        G_OBJECT_CLASS (tcti_device_parent_class)->finalize (obj);
}
/* When the class is initialized we set the pointer to our finalize function.
 */
static void
tcti_device_class_init (gpointer klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    if (tcti_device_parent_class == NULL)
        tcti_device_parent_class = g_type_class_peek_parent (klass);

    object_class->finalize     = tcti_device_finalize;
    object_class->get_property = tcti_device_get_property;
    object_class->set_property = tcti_device_set_property;

    obj_properties[PROP_FILENAME] =
        g_param_spec_string ("filename",
                             "Device file",
                             "TPM device file",
                             "/dev/tpm0",
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
    g_object_class_install_properties (object_class,
                                       N_PROPERTIES,
                                       obj_properties);
}

static void
tcti_device_interface_init (gpointer g_iface)
{
    TctiInterface *tcti_interface = (TctiInterface*)g_iface;
    tcti_interface->get_context = tcti_device_get_context;
    tcti_interface->initialize = tcti_device_initialize;
}
/* Upon first call to *_get_type we register the type with the GType system.
 * We keep a static GType around to speed up future calls.
 */
GType
tcti_device_get_type (void)
{
    static GType type = 0;
    if (type == 0) {
        type = g_type_register_static_simple (G_TYPE_OBJECT,
                                              "TctiDevice",
                                              sizeof (TctiDeviceClass),
                                              (GClassInitFunc) tcti_device_class_init,
                                              sizeof (TctiDevice),
                                              NULL,
                                              0);
        const GInterfaceInfo tcti_info = {
            (GInterfaceInitFunc) tcti_device_interface_init,
            NULL,
            NULL
        };
        g_type_add_interface_static (type, TYPE_TCTI, &tcti_info);
    }
    return type;
}
TctiDevice*
tcti_device_new (const gchar *filename)
{
    return TCTI_DEVICE (g_object_new (TYPE_TCTI_DEVICE,
                                      "filename", filename,
                                      NULL));
}

TSS2_RC
tcti_device_initialize (Tcti *tcti)
{
    TSS2_RC rc;
    size_t ctx_size;
    TctiDevice *self = TCTI_DEVICE (tcti);

    TCTI_DEVICE_CONF config = {
        .device_path = self->filename,
        .logCallback = NULL,
        .logData     = NULL,
    };

    if (self->tcti_context == NULL) {
        rc = InitDeviceTcti (NULL, &ctx_size, NULL);
        if (rc != TSS2_RC_SUCCESS)
            g_error ("failed to get size for device TCTI contexxt structure: "
                     "0x%x", rc);
        self->tcti_context = g_malloc0 (ctx_size);
        rc = InitDeviceTcti (self->tcti_context, &ctx_size, &config);
        if (rc != TSS2_RC_SUCCESS)
            g_free (self->tcti_context);
        return rc;
    }
}
/* Create and expose the internal TCTI context needed by the TSS / SAPI */
TSS2_RC
tcti_device_get_context (Tcti               *tcti,
                         TSS2_TCTI_CONTEXT **ctx)
{
    TSS2_RC rc = TSS2_RC_SUCCESS;
    TctiDevice *self = TCTI_DEVICE (tcti);

    if (self->tcti_context == NULL)
        rc = tcti_device_initialize (tcti);
    *ctx = self->tcti_context;
    return rc;
}

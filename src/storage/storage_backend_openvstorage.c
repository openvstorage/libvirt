/*
 * storage_backend_openvstorage.c: storage backend for OpenvStorage handling
 *
 * Copyright (C) 2016 iNuron NV
 *
 * This file is part of Open vStorage Open Source Edition (OSE),
 * as available from
 *
 *      http://www.openvstorage.org and
 *      http://www.openvstorage.com.
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License v3 (GNU AGPLv3)
 * as published by the Free Software Foundation, in version 3 as it comes in
 * the LICENSE.txt file of the Open vStorage OSE distribution.
 * Open vStorage is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY of any kind.
 *
 * Author: Chrysostomos Nanakos <cnanakos@openvstorage.com>
 */

#include <config.h>

#include "datatypes.h"
#include "virerror.h"
#include "storage_backend_openvstorage.h"
#include "storage_conf.h"
#include "viralloc.h"
#include "virlog.h"
#include "base64.h"
#include "viruuid.h"
#include "virstring.h"

#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <openvstorage/volumedriver.h>

#define VIR_FROM_THIS VIR_FROM_STORAGE
#define OPENVSTORAGE_DFL_PORT   21321

VIR_LOG_INIT("storage.storage_backend_openvstorage");

static int
virStorageBackendOpenvStorageCreateVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                       virStoragePoolObjPtr pool ATTRIBUTE_UNUSED,
                                       virStorageVolDefPtr vol)
{
    if (vol->target.encryption != NULL)
    {
        virReportError(VIR_ERR_CONFIG_UNSUPPORTED, "%s",
                       _("OpenvStorage does not support encrypted volumes"));
        return -1;
    }

    vol->type = VIR_STORAGE_VOL_NETWORK;
    vol->target.format = VIR_STORAGE_FILE_RAW;

    VIR_FREE(vol->key);
    if (virAsprintf(&vol->key, "/%s",
                    vol->name) == -1)
    {
        return -1;
    }

    VIR_FREE(vol->target.path);
    if (VIR_STRDUP(vol->target.path, vol->name) < 0)
    {
        return -1;
    }
    return 0;
}

static int
virStorageBackendOpenvStorageBuildVolHelper(virStoragePoolObjPtr pool,
                                            virStorageVolDefPtr vol,
                                            unsigned int flags,
                                            const char *transport,
                                            bool is_network)
{
    int ret;
    int port = OPENVSTORAGE_DFL_PORT;
    virCheckFlags(0, -1);

    if (is_network) {
        if (pool->def->source.nhost > 0 && pool->def->source.nhost != 1) {
            return -1;
        }
    }

    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    assert(ctx_attr != NULL);

    if (is_network) {
        const char *hostname = pool->def->source.hosts[0].name;
        if (pool->def->source.hosts[0].port) {
            port = pool->def->source.hosts[0].port;
        }
        ret = ovs_ctx_attr_set_transport(ctx_attr,
                                         transport,
                                         hostname,
                                         port);
    } else {
        ret = ovs_ctx_attr_set_transport(ctx_attr,
                                         transport,
                                         NULL,
                                         0);
    }
    if (ret < 0) {
        virReportSystemError(errno, "%s",
                             _("failed to set transport type"));
        ovs_ctx_attr_destroy(ctx_attr);
        return ret;
    }
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    if (ctx == NULL) {
        virReportSystemError(errno, "%s",
                             _("cannot create context"));
        ovs_ctx_attr_destroy(ctx_attr);
        return -1;
    }
    ret = ovs_create_volume(ctx, vol->name, vol->target.capacity);
    ovs_ctx_destroy(ctx);
    ovs_ctx_attr_destroy(ctx_attr);
    return ret;
}

static int
virStorageBackendOpenvStorageBuildVolTCP(virConnectPtr conn ATTRIBUTE_UNUSED,
                                         virStoragePoolObjPtr pool,
                                         virStorageVolDefPtr vol,
                                         unsigned int flags)
{
    return virStorageBackendOpenvStorageBuildVolHelper(pool,
                                                       vol,
                                                       flags,
                                                       "tcp",
                                                       true);
}

static int
virStorageBackendOpenvStorageBuildVolRDMA(virConnectPtr conn ATTRIBUTE_UNUSED,
                                         virStoragePoolObjPtr pool,
                                         virStorageVolDefPtr vol,
                                         unsigned int flags)
{
    return virStorageBackendOpenvStorageBuildVolHelper(pool,
                                                       vol,
                                                       flags,
                                                       "rdma",
                                                       true);
}

static int
virStorageBackendOpenvStorageBuildVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                      virStoragePoolObjPtr pool,
                                      virStorageVolDefPtr vol,
                                      unsigned int flags)
{
    return virStorageBackendOpenvStorageBuildVolHelper(pool,
                                                       vol,
                                                       flags,
                                                       "shm",
                                                       false);
}

static int
virStorageBackendOpenvStorageDeleteVolHelper(virStoragePoolObjPtr pool,
                                             virStorageVolDefPtr vol,
                                             int flags,
                                             const char* transport,
                                             bool is_network)
{
    int ret;
    int port = OPENVSTORAGE_DFL_PORT;
    virCheckFlags(0, -1);

    if (is_network) {
        if (pool->def->source.nhost > 0 && pool->def->source.nhost != 1) {
            return -1;
        }
    }

    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    assert(ctx_attr != NULL);

    if (is_network) {
        const char *hostname = pool->def->source.hosts[0].name;
        if (pool->def->source.hosts[0].port) {
            port = pool->def->source.hosts[0].port;
        }
        ret = ovs_ctx_attr_set_transport(ctx_attr,
                                         transport,
                                         hostname,
                                         port);
    } else {
        ret = ovs_ctx_attr_set_transport(ctx_attr,
                                         transport,
                                         NULL,
                                         0);
    }
    if (ret < 0) {
        virReportSystemError(errno, "%s",
                             _("cannot set transport type"));
        ovs_ctx_attr_destroy(ctx_attr);
        return ret;
    }
    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    if (ctx == NULL) {
        virReportSystemError(errno, "%s",
                             _("failed to create context"));
        ovs_ctx_attr_destroy(ctx_attr);
        return -1;
    }
    ret = ovs_remove_volume(ctx, vol->name);
    ovs_ctx_destroy(ctx);
    ovs_ctx_attr_destroy(ctx_attr);
    return ret;
}

static int
virStorageBackendOpenvStorageDeleteVolTCP(virConnectPtr conn ATTRIBUTE_UNUSED,
                                          virStoragePoolObjPtr pool,
                                          virStorageVolDefPtr vol,
                                          unsigned int flags)
{
    return virStorageBackendOpenvStorageDeleteVolHelper(pool,
                                                        vol,
                                                        flags,
                                                        "tcp",
                                                        true);
}

static int
virStorageBackendOpenvStorageDeleteVolRDMA(virConnectPtr conn ATTRIBUTE_UNUSED,
                                          virStoragePoolObjPtr pool,
                                          virStorageVolDefPtr vol,
                                          unsigned int flags)
{
    return virStorageBackendOpenvStorageDeleteVolHelper(pool,
                                                        vol,
                                                        flags,
                                                        "rdma",
                                                        true);
}

static int
virStorageBackendOpenvStorageDeleteVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                       virStoragePoolObjPtr pool,
                                       virStorageVolDefPtr vol,
                                       unsigned int flags)
{
    return virStorageBackendOpenvStorageDeleteVolHelper(pool,
                                                        vol,
                                                        flags,
                                                        "shm",
                                                        false);
}

static int
virStorageBackendOpenvStorageRefreshVolHelper(virStoragePoolObjPtr pool,
                                              virStorageVolDefPtr vol,
                                              const char *transport,
                                              bool is_network)
{
    int r;
    int port = OPENVSTORAGE_DFL_PORT;
    struct stat st;

    if (is_network) {
        if (pool->def->source.nhost > 0 && pool->def->source.nhost != 1) {
            return -1;
        }
    }

    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    assert(ctx_attr != NULL);

    if (is_network) {
        const char *hostname = pool->def->source.hosts[0].name;
        if (pool->def->source.hosts[0].port) {
            port = pool->def->source.hosts[0].port;
        }
        r = ovs_ctx_attr_set_transport(ctx_attr,
                                       transport,
                                       hostname,
                                       port);
    } else {
        r = ovs_ctx_attr_set_transport(ctx_attr,
                                       transport,
                                       NULL,
                                       0);
    }

    if (r < 0) {
        virReportSystemError(errno, "%s",
                             _("failed to set transport type"));
        ovs_ctx_attr_destroy(ctx_attr);
        return r;
    }

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    if (ctx == NULL) {
        ovs_ctx_attr_destroy(ctx_attr);
        return -1;
    }
    ovs_ctx_attr_destroy(ctx_attr);
    r = ovs_ctx_init(ctx, vol->name, O_RDWR);

    if (r < 0)
    {
        virReportSystemError(errno,
                             _("failed to create context for volume '%s'"),
                             vol->name);
        ovs_ctx_destroy(ctx);
        return r;
    }
    r = ovs_stat(ctx, &st);
    if (r < 0)
    {
        virReportSystemError(errno, _("failed to stat volume '%s'"),
                             vol->name);
        ovs_ctx_destroy(ctx);
        return -1;
    }
    ignore_value(ovs_ctx_destroy(ctx));
    vol->target.capacity = st.st_size;
    vol->target.allocation = st.st_blksize * st.st_blocks;
    vol->type = VIR_STORAGE_VOL_NETWORK;
    vol->target.format = VIR_STORAGE_FILE_RAW;

    VIR_FREE(vol->key);
    if (virAsprintf(&vol->key, "%s",
                    vol->name) == -1)
    {
        virReportSystemError(0, _("cannot copy key for volume '%s'"),
                             vol->name);
        return -1;
    }

    VIR_FREE(vol->target.path);
    ignore_value(VIR_STRDUP(vol->target.path, vol->name));
    return 0;
}

static int
virStorageBackendOpenvStorageRefreshVolTCP(virConnectPtr conn ATTRIBUTE_UNUSED,
                                           virStoragePoolObjPtr pool,
                                           virStorageVolDefPtr vol)
{
    return virStorageBackendOpenvStorageRefreshVolHelper(pool,
                                                         vol,
                                                         "tcp",
                                                         true);
}

static int
virStorageBackendOpenvStorageRefreshVolRDMA(virConnectPtr conn ATTRIBUTE_UNUSED,
                                            virStoragePoolObjPtr pool,
                                            virStorageVolDefPtr vol)
{
    return virStorageBackendOpenvStorageRefreshVolHelper(pool,
                                                         vol,
                                                         "rdma",
                                                         true);
}

static int
virStorageBackendOpenvStorageRefreshVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                        virStoragePoolObjPtr pool,
                                        virStorageVolDefPtr vol)
{
    return virStorageBackendOpenvStorageRefreshVolHelper(pool,
                                                         vol,
                                                         "shm",
                                                         false);
}

static int
virStorageBackendOpenvStorageRefreshPoolHelper(virConnectPtr conn,
                                               virStoragePoolObjPtr pool,
                                               const char *transport,
                                               bool is_network)
{
    const uint64_t fs_size = 64ULL << 40;
    size_t max_size = 1024;
    char *name, *names = NULL;
    int len = -1;
    int r = -1;
    int port = OPENVSTORAGE_DFL_PORT;
    pool->def->capacity = fs_size;
    pool->def->available = fs_size;
    pool->def->capacity = fs_size;

    if (is_network) {
        if (pool->def->source.nhost > 0 && pool->def->source.nhost != 1) {
            return -1;
        }
    }

    ovs_ctx_attr_t *ctx_attr = ovs_ctx_attr_new();
    assert(ctx_attr != NULL);
    if (is_network) {
        const char *hostname = pool->def->source.hosts[0].name;
        if (pool->def->source.hosts[0].port) {
            port = pool->def->source.hosts[0].port;
        }
        r = ovs_ctx_attr_set_transport(ctx_attr,
                                       transport,
                                       hostname,
                                       port);
    } else {
        r = ovs_ctx_attr_set_transport(ctx_attr,
                                       transport,
                                       NULL,
                                       0);
    }
    if (r < 0) {
        virReportSystemError(errno, "%s",
                             _("failed to set transport type"));
        ovs_ctx_attr_destroy(ctx_attr);
        return r;
    }

    ovs_ctx_t *ctx = ovs_ctx_new(ctx_attr);
    if (ctx == NULL) {
        virReportSystemError(errno, "%s",
                             _("failed to create context"));
        ovs_ctx_attr_destroy(ctx_attr);
        return -1;
    }
    ovs_ctx_attr_destroy(ctx_attr);

    while (true)
    {
        if (VIR_ALLOC_N(names, max_size) < 0)
            goto cleanup;
        len = ovs_list_volumes(ctx, names, &max_size);
        if (len >= 0)
            break;
        if (len == -1 && errno != ERANGE)
        {
            virReportSystemError(errno, "%s",
                                 _("A problem occured while listing images"));
            goto cleanup;
        }
        VIR_FREE(names);
    }

    ignore_value(ovs_ctx_destroy(ctx));
    for (name = names; name < names + max_size;) {
        virStorageVolDefPtr vol;

        if (VIR_REALLOC_N(pool->volumes.objs, pool->volumes.count + 1) < 0) {
            virStoragePoolObjClearVols(pool);
            goto cleanup;
        }

        if (STREQ(name, ""))
            break;

        if (VIR_ALLOC(vol) < 0)
            goto cleanup;

        if (VIR_STRDUP(vol->name, name) < 0) {
            VIR_FREE(vol);
            goto cleanup;
        }

        name += strlen(name) + 1;

        if (virStorageBackendOpenvStorageRefreshVol(conn, pool, vol) < 0) {
            VIR_WARN("%s", _("cannot refresh volume info"));
            virStorageVolDefFree(vol);
            goto cleanup;
        }
        pool->volumes.objs[pool->volumes.count++] = vol;
    }
    r = 0;

cleanup:
    VIR_FREE(names);
    return r;
}

static int
virStorageBackendOpenvStorageRefreshPoolTCP(virConnectPtr conn,
                                            virStoragePoolObjPtr pool)
{
    return virStorageBackendOpenvStorageRefreshPoolHelper(conn,
                                                          pool,
                                                          "tcp",
                                                          true);
}

static int
virStorageBackendOpenvStorageRefreshPoolRDMA(virConnectPtr conn,
                                             virStoragePoolObjPtr pool)
{
    return virStorageBackendOpenvStorageRefreshPoolHelper(conn,
                                                          pool,
                                                          "rdma",
                                                          true);
}

static int
virStorageBackendOpenvStorageRefreshPool(virConnectPtr conn,
                                         virStoragePoolObjPtr pool)
{
    return virStorageBackendOpenvStorageRefreshPoolHelper(conn,
                                                          pool,
                                                          "shm",
                                                          false);
}

virStorageBackend virStorageBackendOpenvStorage = {
    .type = VIR_STORAGE_POOL_OPENVSTORAGE,

    .refreshPool = virStorageBackendOpenvStorageRefreshPool,
    .createVol = virStorageBackendOpenvStorageCreateVol,
    .buildVol = virStorageBackendOpenvStorageBuildVol,
    .refreshVol = virStorageBackendOpenvStorageRefreshVol,
    .deleteVol = virStorageBackendOpenvStorageDeleteVol,
};

virStorageBackend virStorageBackendOpenvStorageTCP = {
    .type = VIR_STORAGE_POOL_OPENVSTORAGE_TCP,

    .refreshPool = virStorageBackendOpenvStorageRefreshPoolTCP,
    .createVol = virStorageBackendOpenvStorageCreateVol,
    .buildVol = virStorageBackendOpenvStorageBuildVolTCP,
    .refreshVol = virStorageBackendOpenvStorageRefreshVolTCP,
    .deleteVol = virStorageBackendOpenvStorageDeleteVolTCP,
};

virStorageBackend virStorageBackendOpenvStorageRDMA = {
    .type = VIR_STORAGE_POOL_OPENVSTORAGE_RDMA,

    .refreshPool = virStorageBackendOpenvStorageRefreshPoolRDMA,
    .createVol = virStorageBackendOpenvStorageCreateVol,
    .buildVol = virStorageBackendOpenvStorageBuildVolRDMA,
    .refreshVol = virStorageBackendOpenvStorageRefreshVolRDMA,
    .deleteVol = virStorageBackendOpenvStorageDeleteVolRDMA,
};

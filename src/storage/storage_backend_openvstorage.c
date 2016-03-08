/*
 * storage_backend_openvstorage.c: storage backend for OpenvStorage handling
 *
 * Copyright (C) 2016 iNuron NV.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <http://www.gnu.org/licenses/>.
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
#include <openvstorage/volumedriver.h>

#define VIR_FROM_THIS VIR_FROM_STORAGE

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
virStorageBackendOpenvStorageBuildVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                      virStoragePoolObjPtr pool ATTRIBUTE_UNUSED,
                                      virStorageVolDefPtr vol,
                                      unsigned int flags)
{
    virCheckFlags(0, -1);
    return ovs_create_volume(vol->name, vol->capacity);
}

static int
virStorageBackendOpenvStorageDeleteVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                       virStoragePoolObjPtr pool ATTRIBUTE_UNUSED,
                                       virStorageVolDefPtr vol,
                                       unsigned int flags)
{
    virCheckFlags(0, -1);
    return ovs_remove_volume(vol->name);
}

static int
virStorageBackendOpenvStorageRefreshVol(virConnectPtr conn ATTRIBUTE_UNUSED,
                                        virStoragePoolObjPtr pool ATTRIBUTE_UNUSED,
                                        virStorageVolDefPtr vol)
{
    int r;
    struct stat st;
    ovs_ctx_t *ioctx = ovs_ctx_init(vol->name, O_RDWR);

    if (ioctx == NULL)
    {
        virReportSystemError(0, _("failed to create context for volume '%s'"),
                             vol->name);
        return -1;
    }
    r = ovs_stat(ioctx, &st);
    if (r < 0)
    {
        virReportSystemError(0, _("failed to stat volume '%s'"),
                             vol->name);
        ovs_ctx_destroy(ioctx);
        return -1;
    }
    ignore_value(ovs_ctx_destroy(ioctx));
    vol->capacity = st.st_size;
    /*vol->allocation = st.st_blksize * st.st_blocks;*/
    vol->type = VIR_STORAGE_VOL_NETWORK;

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
virStorageBackendOpenvStorageRefreshPool(virConnectPtr conn,
                                         virStoragePoolObjPtr pool)
{
    const uint64_t fs_size = 64ULL << 40;
    size_t max_size = 1024;
    char *name, *names = NULL;
    int len = -1;
    int r = -1;
    pool->def->capacity = fs_size;
    pool->def->available = fs_size;
    pool->def->capacity = fs_size;

    while (true)
    {
        if (VIR_ALLOC_N(names, max_size) < 0)
            goto cleanup;

        len = ovs_list_volumes(names, &max_size);
        if (len >= 0)
            break;
        if (len == -1 && errno != ERANGE)
        {
            virReportSystemError(errno, "%s",
                                 _("A problem occured while listing OpenvStorage images"));
            goto cleanup;
        }
        VIR_FREE(names);
    }

    for (name = names; name < names + max_size;) {
        virStorageVolDefPtr vol;

        if (VIR_REALLOC_N(pool->volumes.objs, pool->volumes.count + 1) < 0) {
            VIR_WARN("%s", _("cannot allocate volume objs"));
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


virStorageBackend virStorageBackendOpenvStorage = {
    .type = VIR_STORAGE_POOL_OPENVSTORAGE,

    .refreshPool = virStorageBackendOpenvStorageRefreshPool,
    .createVol = virStorageBackendOpenvStorageCreateVol,
    .buildVol = virStorageBackendOpenvStorageBuildVol,
    .refreshVol = virStorageBackendOpenvStorageRefreshVol,
    .deleteVol = virStorageBackendOpenvStorageDeleteVol,
};

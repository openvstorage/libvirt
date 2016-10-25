/*
 * storage_backend_openvstorage.h: storage backend for OpenvStorage handling
 *
 * Copyright (C) 2016 iNuron NV
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

#ifndef __VIR_STORAGE_BACKEND_OPENVSTORAGE_H__
# define __VIR_STORAGE_BACKEND_OPENVSTORAGE_H__

# include "storage_backend.h"

extern virStorageBackend virStorageBackendOpenvStorage;
extern virStorageBackend virStorageBackendOpenvStorageTCP;
extern virStorageBackend virStorageBackendOpenvStorageRDMA;

#endif /* __VIR_STORAGE_BACKEND_OPENVSTORAGE_H__ */

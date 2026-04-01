# OpenArangoDBCore User Guide

This guide covers installation, configuration, and usage of all 19 Enterprise modules provided by OpenArangoDBCore.

---

## Table of Contents

1. [Installation](#installation)
2. [Verifying Enterprise Features](#verifying-enterprise-features)
3. [Module Configuration Reference](#module-configuration-reference)
4. [Module Usage Examples](#module-usage-examples)
5. [Troubleshooting](#troubleshooting)

---

## Installation

### Step 1: Clone ArangoDB

```bash
git clone https://github.com/arangodb/arangodb.git
cd arangodb
```

Use the latest stable release branch for production deployments:

```bash
git checkout v3.12.0   # or latest stable tag
```

### Step 2: Clone OpenArangoDBCore as the Enterprise Directory

```bash
git clone https://github.com/bolnet/OpenArangoDBCore.git enterprise
```

ArangoDB expects an `enterprise/` directory at the repository root. OpenArangoDBCore provides the exact headers and symbols that ArangoDB links against when Enterprise mode is enabled.

### Step 3: Install Build Dependencies

**Ubuntu / Debian:**
```bash
sudo apt-get update
sudo apt-get install -y \
  cmake g++ libssl-dev libldap2-dev \
  python3 python3-pip \
  libsnappy-dev liblz4-dev libzstd-dev
```

**macOS (Homebrew):**
```bash
brew install cmake openssl@3 openldap snappy lz4 zstd
```

**RHEL / CentOS / Fedora:**
```bash
sudo dnf install -y \
  cmake gcc-c++ openssl-devel openldap-devel \
  snappy-devel lz4-devel libzstd-devel
```

### Step 4: Configure and Build

```bash
mkdir build && cd build

cmake .. \
  -DUSE_ENTERPRISE=1 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/usr/local

make -j$(nproc)
sudo make install
```

### Step 5: Start ArangoDB

```bash
arangod \
  --server.endpoint tcp://0.0.0.0:8529 \
  --database.directory /var/lib/arangodb3
```

---

## Verifying Enterprise Features

After starting ArangoDB, verify that Enterprise features are active:

```bash
# Via arangosh
arangosh --server.endpoint tcp://127.0.0.1:8529 \
  --javascript.execute-string "print(require('@arangodb').isEnterprise())"
# Output: true

# Via REST API
curl -s http://127.0.0.1:8529/_admin/license | jq .
# Should show license status with all features enabled
```

---

## Module Configuration Reference

### License Feature

No configuration required. The license module always reports a valid, perpetual license for all Enterprise features.

| Flag | Description | Default |
|------|-------------|---------|
| *(none)* | License is always valid | Enabled |

---

### Encryption at Rest

Encrypts all RocksDB data (SST files, WAL, MANIFEST) using AES-256-CTR.

| Flag | Description | Default |
|------|-------------|---------|
| `--rocksdb.encryption-keyfile` | Path to a 32-byte encryption key file | *(disabled)* |
| `--rocksdb.encryption-keyfolder` | Directory containing versioned keys for rotation | *(disabled)* |
| `--rocksdb.encryption-key-rotation` | Enable key rotation API | `false` |

**Generating an encryption key:**
```bash
openssl rand 32 > /etc/arangodb3/encryption-key
chmod 600 /etc/arangodb3/encryption-key
```

**Starting with encryption:**
```bash
arangod --rocksdb.encryption-keyfile /etc/arangodb3/encryption-key
```

**Key rotation (runtime):**
```bash
# Place the new key in the keyfolder, then trigger rotation
curl -X PUT http://127.0.0.1:8529/_admin/encryption \
  -H "Authorization: bearer $JWT"
```

---

### Audit Logging

Records structured audit events to a file or syslog.

| Flag | Description | Default |
|------|-------------|---------|
| `--audit.output` | Output target: file path or `syslog` | *(disabled)* |
| `--audit.topics` | Comma-separated list of enabled topics | All topics |
| `--audit.hostname` | Hostname to include in audit records | System hostname |

**Available topics:** `audit-authentication`, `audit-authorization`, `audit-collection`, `audit-database`, `audit-document`, `audit-view`, `audit-service`, `audit-hotbackup`

**Example:**
```bash
arangod \
  --audit.output /var/log/arangodb3/audit.log \
  --audit.topics "audit-authentication,audit-authorization,audit-collection"
```

---

### LDAP Authentication

Delegates user authentication to an external LDAP/Active Directory server.

| Flag | Description | Default |
|------|-------------|---------|
| `--ldap.enabled` | Enable LDAP authentication | `false` |
| `--ldap.url` | LDAP server URL (e.g., `ldap://ldap.example.com:389`) | *(required)* |
| `--ldap.prefix` | DN prefix for simple bind (e.g., `uid=`) | `""` |
| `--ldap.suffix` | DN suffix for simple bind (e.g., `,dc=example,dc=com`) | `""` |
| `--ldap.search-filter` | LDAP search filter for search mode | `""` |
| `--ldap.search-attribute` | Attribute containing the username | `uid` |
| `--ldap.basedn` | Base DN for user searches | *(required for search mode)* |
| `--ldap.binddn` | Service account DN for search mode | `""` |
| `--ldap.bindpasswd` | Service account password | `""` |
| `--ldap.roles-attribute-name` | LDAP attribute containing role memberships | `memberOf` |
| `--ldap.tls` | Enable TLS for LDAP connection | `false` |
| `--ldap.refresh-rate` | Role cache refresh interval (seconds) | `300` |

**Simple bind mode:**
```bash
arangod \
  --ldap.enabled true \
  --ldap.url "ldap://ldap.example.com:389" \
  --ldap.prefix "uid=" \
  --ldap.suffix ",ou=users,dc=example,dc=com" \
  --ldap.tls true
```

**Search bind mode:**
```bash
arangod \
  --ldap.enabled true \
  --ldap.url "ldap://ldap.example.com:389" \
  --ldap.basedn "ou=users,dc=example,dc=com" \
  --ldap.search-filter "(objectClass=person)" \
  --ldap.search-attribute "uid" \
  --ldap.binddn "cn=readonly,dc=example,dc=com" \
  --ldap.bindpasswd "service-password" \
  --ldap.roles-attribute-name "memberOf"
```

---

### Data Masking

Applies masking transformations during `arangodump` for anonymized exports.

| Flag | Description | Default |
|------|-------------|---------|
| `--maskings` | Path to JSON masking rules file | *(disabled)* |

**Masking types:** `xifyFront`, `randomString`, `creditCard`, `phoneNumber`, `email`, `null`, `zipcode`, `identity`

**Example masking config (`masking-rules.json`):**
```json
{
  "collections": {
    "users": {
      "rules": [
        { "path": "email", "type": "email" },
        { "path": "name", "type": "xifyFront" },
        { "path": "ssn", "type": "null" },
        { "path": "phone", "type": "phoneNumber" },
        { "path": "billing.creditCard", "type": "creditCard" }
      ]
    }
  }
}
```

**Running a masked dump:**
```bash
arangodump \
  --server.endpoint tcp://127.0.0.1:8529 \
  --output-directory /tmp/masked-dump \
  --maskings masking-rules.json
```

---

### Enhanced SSL/TLS

Extends TLS with SNI support, hot reload, and cipher control.

| Flag | Description | Default |
|------|-------------|---------|
| `--ssl.keyfile` | Path to PEM-encoded private key + certificate | *(required for TLS)* |
| `--ssl.cafile` | Path to CA certificate for client verification | *(disabled)* |
| `--ssl.protocol` | Minimum TLS version (`4` = TLS 1.2, `5` = TLS 1.3) | `5` |
| `--ssl.cipher-list` | OpenSSL cipher string for TLS 1.2 | System default |
| `--ssl.ciphersuites` | TLS 1.3 ciphersuites | System default |
| `--ssl.sni.*` | Additional SNI-specific key/cert pairs | *(disabled)* |

**Enabling mTLS:**
```bash
arangod \
  --ssl.keyfile /etc/arangodb3/server.pem \
  --ssl.cafile /etc/arangodb3/ca.pem \
  --ssl.protocol 5 \
  --server.endpoint ssl://0.0.0.0:8529
```

**Hot reload (no restart):**
```bash
curl -X POST https://127.0.0.1:8529/_admin/ssl/reload \
  -H "Authorization: bearer $JWT"
```

---

### SmartGraphs

Partitions graph data across shards by a user-defined attribute for zero-hop traversals.

| Flag | Description | Default |
|------|-------------|---------|
| *(graph option)* `smartGraphAttribute` | Vertex attribute used as partition key | *(required)* |
| *(graph option)* `numberOfShards` | Number of shards | `1` |
| *(graph option)* `isDisjoint` | Enable disjoint mode (no cross-partition edges) | `false` |

**Creating a SmartGraph:**
```javascript
// In arangosh
var graph_module = require("@arangodb/smart-graph");
graph_module._create("mySmartGraph", [
  graph_module._relation("edges", ["vertices"], ["vertices"])
], [], {
  smartGraphAttribute: "region",
  numberOfShards: 4
});
```

**Creating a Disjoint SmartGraph:**
```javascript
graph_module._create("myDisjointGraph", [
  graph_module._relation("edges", ["vertices"], ["vertices"])
], [], {
  smartGraphAttribute: "region",
  numberOfShards: 4,
  isDisjoint: true
});
```

**Vertex key format:** Documents must use `<smartGraphAttribute>:<uniqueId>` as the `_key`, e.g., `us-east:user123`.

---

### Satellite Collections / SatelliteGraphs

Replicate small collections to every DB-Server for local joins.

**Creating a Satellite Collection:**
```javascript
db._create("lookupTable", { replicationFactor: "satellite" });
```

**Creating a SatelliteGraph:**
```javascript
var graph_module = require("@arangodb/satellite-graph");
graph_module._create("mySatelliteGraph", [
  graph_module._relation("edges", ["vertices"], ["vertices"])
]);
```

All vertex and edge collections are automatically replicated to every DB-Server.

---

### Shard-local Graph Execution

No configuration required. The AQL optimizer automatically detects SmartGraph traversals with a deterministic start vertex and rewrites the query plan to use `LocalTraversalNode`, executing the traversal entirely on the responsible DB-Server.

**Verifying shard-local execution:**
```javascript
db._explain("FOR v, e IN 1..3 OUTBOUND 'vertices/us-east:user1' GRAPH 'mySmartGraph' RETURN v");
// Look for "LocalTraversalNode" in the plan (instead of "TraversalNode")
```

---

### ReadFromFollower

Routes read queries to follower replicas for horizontal read scaling.

| Flag | Description | Default |
|------|-------------|---------|
| *(request header)* `X-Arango-Allow-Dirty-Read` | Enable per-request dirty reads | `false` |

**Usage:**
```bash
curl -H "X-Arango-Allow-Dirty-Read: true" \
  http://127.0.0.1:8529/_db/mydb/_api/document/collection/key
```

Responses include `X-Arango-Potential-Dirty-Read: true` when a follower was used.

---

### MinHash Similarity Functions

AQL functions for approximate Jaccard similarity estimation.

**AQL functions:**
```aql
-- Compute MinHash signature
LET sig = MINHASH(document.text, 128)

-- Find similar documents
FOR doc IN myView
  SEARCH MINHASH_MATCH(doc.text, "search text", 0.5, "minhash_analyzer")
  RETURN doc

-- Exact Jaccard similarity between two arrays
RETURN JACCARD([1,2,3], [2,3,4])
// => 0.5
```

**Creating a MinHash analyzer:**
```javascript
var analyzers = require("@arangodb/analyzers");
analyzers.save("minhash_analyzer", "minhash", {
  analyzer: { type: "text", properties: { locale: "en" } },
  numHashes: 128
});
```

---

### TopK / WAND Query Optimization

Accelerates ranked search queries with early termination.

**Enable in view definition:**
```javascript
db._createView("myView", "arangosearch", {
  links: { "documents": { includeAllFields: true } },
  optimizeTopK: ["BM25(@doc) DESC"]
});
```

**Query (automatically optimized):**
```aql
FOR doc IN myView
  SEARCH ANALYZER(doc.text == "search terms", "text_en")
  SORT BM25(doc) DESC
  LIMIT 10
  RETURN doc
```

Verify optimization with `db._explain()` -- look for the TopK/WAND rule.

---

### Hot Backup

Creates near-instantaneous, consistent snapshots without downtime.

**REST API:**
```bash
# Create a backup
curl -X POST http://127.0.0.1:8529/_admin/backup/create \
  -H "Authorization: bearer $JWT" \
  -d '{"label": "pre-deploy-2026-04-01"}'

# List backups
curl http://127.0.0.1:8529/_admin/backup/list \
  -H "Authorization: bearer $JWT"

# Restore from backup
curl -X POST http://127.0.0.1:8529/_admin/backup/restore \
  -H "Authorization: bearer $JWT" \
  -d '{"id": "2026-04-01T12.00.00Z_pre-deploy-2026-04-01"}'

# Delete a backup
curl -X DELETE http://127.0.0.1:8529/_admin/backup/delete \
  -H "Authorization: bearer $JWT" \
  -d '{"id": "2026-04-01T12.00.00Z_pre-deploy-2026-04-01"}'
```

---

### Cloud Backup (RClone)

Uploads hot backup snapshots to cloud object storage.

**Example: Upload to S3:**
```bash
arangobackup upload \
  --rclone-config-file remote.json \
  --remote-path s3://my-bucket/backups \
  --backup-id "2026-04-01T12.00.00Z_pre-deploy-2026-04-01"
```

**Example `remote.json` for S3:**
```json
{
  "type": "s3",
  "provider": "AWS",
  "access_key_id": "AKIA...",
  "secret_access_key": "...",
  "region": "us-east-1"
}
```

**Supported providers:** Amazon S3, Azure Blob Storage, Google Cloud Storage, and 40+ rclone backends.

---

### Parallel Index Building

Builds indexes using multiple threads. Background builds do not block reads.

```javascript
// Foreground parallel build
db.largeCollection.ensureIndex({
  type: "persistent",
  fields: ["category", "timestamp"]
});

// Background build (does not block CRUD)
db.largeCollection.ensureIndex({
  type: "persistent",
  fields: ["category", "timestamp"],
  inBackground: true
});
```

---

### DC-to-DC Replication

Asynchronous cross-datacenter replication for disaster recovery.

| Flag | Description | Default |
|------|-------------|---------|
| `--replication.dc2dc.enabled` | Enable DC2DC replication | `false` |
| `--replication.dc2dc.source` | Source cluster endpoint | *(required)* |
| `--replication.dc2dc.target` | Target cluster endpoint | *(required)* |
| `--replication.dc2dc.tls-keyfile` | mTLS client key/cert | *(required)* |
| `--replication.dc2dc.tls-cafile` | CA certificate for mTLS | *(required)* |

**Setup with arangosync:**
```bash
# On the source datacenter
arangosync configure sync \
  --source.endpoint=https://source-coordinator:8529 \
  --target.endpoint=https://target-coordinator:8529 \
  --auth.keyfile /etc/arangodb3/dc2dc-key.pem \
  --auth.cacert /etc/arangodb3/ca.pem

# Start replication
arangosync start sync
```

All databases, collections, indexes, views, users, and Foxx services replicate automatically.

---

## Troubleshooting

### Build fails with "USE_ENTERPRISE not defined"

Ensure you pass `-DUSE_ENTERPRISE=1` to CMake and that the `enterprise/` directory exists at the ArangoDB repository root:

```bash
ls arangodb/enterprise/CMakeLists.txt   # must exist
cmake .. -DUSE_ENTERPRISE=1
```

### Unresolved symbols at link time

This typically means the Enterprise directory structure does not match what ArangoDB expects. Verify that the `Enterprise/` subdirectory layout matches the expected namespace layout:

```bash
# Check that Enterprise headers are found
grep -r "Enterprise/" build/CMakeCache.txt
```

### Encryption key file errors

- The key file must be exactly 32 bytes (256 bits)
- The file must be readable by the `arangod` process
- Do not include a trailing newline: `openssl rand 32 > keyfile` (not `echo`)

```bash
wc -c /etc/arangodb3/encryption-key
# Must output: 32
```

### LDAP connection failures

- Verify the LDAP URL is reachable: `ldapsearch -x -H ldap://ldap.example.com:389 -b "dc=example,dc=com"`
- If using TLS, ensure the CA certificate is trusted or set `--ldap.tls-ca-cert`
- Check that `--ldap.prefix` and `--ldap.suffix` form a valid DN: `uid=<username>,ou=users,dc=example,dc=com`

### Audit log not producing output

- Verify `--audit.output` is set to a writable file path
- Check that at least one topic is enabled with `--audit.topics`
- Audit logging is asynchronous; events may appear with a brief delay

### SmartGraph key validation errors

Vertex documents in SmartGraph collections must use the format `<smartGraphAttribute>:<uniqueId>` as their `_key`. For example, with `smartGraphAttribute: "region"`:

```
Valid:   { "_key": "us-east:user123", "region": "us-east", ... }
Invalid: { "_key": "user123", "region": "us-east", ... }        // missing prefix
Invalid: { "_key": "us-east:user123", "region": "eu-west", ... } // prefix mismatch
```

### Hot backup restore fails

- Ensure no other ArangoDB instance is using the same data directory
- The backup ID must match exactly (case-sensitive, includes timestamp)
- Encrypted backups require the same encryption key that was active during backup

### Cloud backup upload hangs

- Verify rclone credentials in the config file
- Test rclone independently: `rclone ls remote:bucket-name --config remote.json`
- Check network connectivity to the cloud storage endpoint

### DC2DC replication lag

- Verify mTLS certificates are valid and not expired
- Check that both source and target clusters are healthy
- Monitor replication lag via the arangosync status endpoint
- Network partitions are handled gracefully; replication resumes automatically after connectivity is restored

### Tests fail with AddressSanitizer errors

Run with full ASan output for diagnostics:

```bash
ASAN_OPTIONS=detect_odr_violation=2:halt_on_error=0 ctest --output-on-failure
```

# OpenArangoDBCore Deployment Guide

This guide covers deployment scenarios for ArangoDB built with OpenArangoDBCore Enterprise features.

---

## Table of Contents

1. [Single-Server Deployment](#single-server-deployment)
2. [Cluster Deployment](#cluster-deployment)
3. [Docker Deployment](#docker-deployment)
4. [Production Checklist](#production-checklist)

---

## Single-Server Deployment

A single-server deployment is the simplest option. All Enterprise features are available except those that require a cluster (ReadFromFollower, DC-to-DC replication).

### Build and Install

```bash
# Clone and build
git clone https://github.com/arangodb/arangodb.git
cd arangodb
git clone https://github.com/bolnet/OpenArangoDBCore.git enterprise

mkdir build && cd build
cmake .. \
  -DUSE_ENTERPRISE=1 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/opt/arangodb

make -j$(nproc)
make install
```

### Configure

Create the configuration file at `/etc/arangodb3/arangod.conf`:

```ini
[server]
endpoint = ssl://0.0.0.0:8529
storage-engine = rocksdb

[database]
directory = /var/lib/arangodb3

[log]
level = info
file = /var/log/arangodb3/arangod.log

[ssl]
keyfile = /etc/arangodb3/server.pem
cafile = /etc/arangodb3/ca.pem
protocol = 5

[rocksdb]
encryption-keyfile = /etc/arangodb3/encryption-key

[audit]
output = /var/log/arangodb3/audit.log
topics = audit-authentication,audit-authorization,audit-collection,audit-database
```

### Create a systemd Service

```ini
# /etc/systemd/system/arangodb3.service
[Unit]
Description=ArangoDB Server (Enterprise via OpenArangoDBCore)
After=network.target

[Service]
Type=simple
User=arangodb
Group=arangodb
ExecStart=/opt/arangodb/bin/arangod --config /etc/arangodb3/arangod.conf
Restart=on-failure
RestartSec=5
LimitNOFILE=65536

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable arangodb3
sudo systemctl start arangodb3
```

### Verify

```bash
curl -k https://127.0.0.1:8529/_api/version
# {"server":"arango","version":"3.12.0","license":"enterprise"}
```

---

## Cluster Deployment

A production ArangoDB cluster consists of three roles: Agent, Coordinator, and DB-Server. All roles use the same binary built with OpenArangoDBCore.

### Minimum Cluster Topology

| Role | Count | Purpose |
|------|-------|---------|
| Agent | 3 | Consensus store (Raft). Stores cluster metadata. |
| Coordinator | 2+ | Stateless query routers. Clients connect here. |
| DB-Server | 3+ | Data storage. Shards live here. |

### Network Requirements

| Port | Usage |
|------|-------|
| 8529 | Coordinator (client-facing) |
| 8530 | DB-Server (internal cluster) |
| 8531 | Agent (internal cluster) |

All internal cluster communication should use mTLS.

### Step 1: Generate TLS Certificates

```bash
# Generate CA
openssl req -x509 -newkey rsa:4096 -days 3650 -nodes \
  -keyout ca-key.pem -out ca.pem \
  -subj "/CN=ArangoDB Cluster CA"

# Generate server cert (repeat per node with correct CN/SAN)
openssl req -newkey rsa:4096 -nodes \
  -keyout server-key.pem -out server-csr.pem \
  -subj "/CN=arangodb-node1" \
  -addext "subjectAltName=DNS:arangodb-node1,IP:10.0.1.1"

openssl x509 -req -in server-csr.pem -CA ca.pem -CAkey ca-key.pem \
  -CAcreateserial -days 365 -out server-cert.pem

# Combine key + cert into PEM file
cat server-key.pem server-cert.pem > server.pem
```

### Step 2: Generate Encryption Key

```bash
# Same key on all nodes (or use keyfolder for per-node keys)
openssl rand 32 > /etc/arangodb3/encryption-key
chmod 600 /etc/arangodb3/encryption-key
```

### Step 3: Start Agents

```bash
# On agent1, agent2, agent3
arangod \
  --cluster.agency-endpoint tcp://agent1:8531 \
  --cluster.agency-endpoint tcp://agent2:8531 \
  --cluster.agency-endpoint tcp://agent3:8531 \
  --cluster.agency-size 3 \
  --cluster.my-role AGENT \
  --cluster.my-address tcp://$(hostname):8531 \
  --server.endpoint ssl://0.0.0.0:8531 \
  --ssl.keyfile /etc/arangodb3/server.pem \
  --ssl.cafile /etc/arangodb3/ca.pem \
  --rocksdb.encryption-keyfile /etc/arangodb3/encryption-key \
  --database.directory /var/lib/arangodb3-agent
```

### Step 4: Start DB-Servers

```bash
# On dbserver1, dbserver2, dbserver3
arangod \
  --cluster.agency-endpoint tcp://agent1:8531 \
  --cluster.agency-endpoint tcp://agent2:8531 \
  --cluster.agency-endpoint tcp://agent3:8531 \
  --cluster.my-role DBSERVER \
  --cluster.my-address tcp://$(hostname):8530 \
  --server.endpoint ssl://0.0.0.0:8530 \
  --ssl.keyfile /etc/arangodb3/server.pem \
  --ssl.cafile /etc/arangodb3/ca.pem \
  --rocksdb.encryption-keyfile /etc/arangodb3/encryption-key \
  --database.directory /var/lib/arangodb3-dbserver \
  --audit.output /var/log/arangodb3/audit.log
```

### Step 5: Start Coordinators

```bash
# On coordinator1, coordinator2
arangod \
  --cluster.agency-endpoint tcp://agent1:8531 \
  --cluster.agency-endpoint tcp://agent2:8531 \
  --cluster.agency-endpoint tcp://agent3:8531 \
  --cluster.my-role COORDINATOR \
  --cluster.my-address tcp://$(hostname):8529 \
  --server.endpoint ssl://0.0.0.0:8529 \
  --ssl.keyfile /etc/arangodb3/server.pem \
  --ssl.cafile /etc/arangodb3/ca.pem \
  --audit.output /var/log/arangodb3/audit.log
```

### Step 6: Verify Cluster Health

```bash
curl -k https://coordinator1:8529/_admin/cluster/health | jq .
```

All nodes should report status `GOOD`.

---

## Docker Deployment

### Dockerfile

```dockerfile
# Stage 1: Build
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake g++ git libssl-dev libldap2-dev \
    python3 libsnappy-dev liblz4-dev libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
RUN git clone --depth 1 --branch v3.12.0 https://github.com/arangodb/arangodb.git
WORKDIR /src/arangodb
RUN git clone --depth 1 https://github.com/bolnet/OpenArangoDBCore.git enterprise

RUN mkdir build && cd build && \
    cmake .. \
      -DUSE_ENTERPRISE=1 \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_INSTALL_PREFIX=/opt/arangodb && \
    make -j$(nproc) && \
    make install

# Stage 2: Runtime
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libssl3 libldap-2.5-0 libsnappy1v5 liblz4-1 libzstd1 \
    curl jq \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/arangodb /opt/arangodb

RUN useradd -r -s /sbin/nologin arangodb && \
    mkdir -p /var/lib/arangodb3 /var/log/arangodb3 /etc/arangodb3 && \
    chown -R arangodb:arangodb /var/lib/arangodb3 /var/log/arangodb3

ENV PATH="/opt/arangodb/bin:${PATH}"

USER arangodb
EXPOSE 8529

ENTRYPOINT ["arangod"]
CMD ["--server.endpoint", "tcp://0.0.0.0:8529", \
     "--database.directory", "/var/lib/arangodb3"]
```

### docker-compose.yml (Single Server)

```yaml
version: "3.8"

services:
  arangodb:
    build: .
    ports:
      - "8529:8529"
    volumes:
      - arangodb-data:/var/lib/arangodb3
      - ./encryption-key:/etc/arangodb3/encryption-key:ro
      - ./server.pem:/etc/arangodb3/server.pem:ro
      - ./ca.pem:/etc/arangodb3/ca.pem:ro
    command:
      - "--server.endpoint=ssl://0.0.0.0:8529"
      - "--database.directory=/var/lib/arangodb3"
      - "--ssl.keyfile=/etc/arangodb3/server.pem"
      - "--ssl.cafile=/etc/arangodb3/ca.pem"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
      - "--audit.output=/var/log/arangodb3/audit.log"

volumes:
  arangodb-data:
```

### docker-compose.yml (3-Node Cluster)

```yaml
version: "3.8"

x-common: &common
  build: .
  volumes:
    - ./certs:/etc/arangodb3/certs:ro
    - ./encryption-key:/etc/arangodb3/encryption-key:ro

services:
  agent1:
    <<: *common
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.agency-size=3"
      - "--cluster.my-role=AGENT"
      - "--cluster.my-address=tcp://agent1:8531"
      - "--server.endpoint=tcp://0.0.0.0:8531"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
    volumes:
      - agent1-data:/var/lib/arangodb3

  agent2:
    <<: *common
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.agency-size=3"
      - "--cluster.my-role=AGENT"
      - "--cluster.my-address=tcp://agent2:8531"
      - "--server.endpoint=tcp://0.0.0.0:8531"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
    volumes:
      - agent2-data:/var/lib/arangodb3

  agent3:
    <<: *common
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.agency-size=3"
      - "--cluster.my-role=AGENT"
      - "--cluster.my-address=tcp://agent3:8531"
      - "--server.endpoint=tcp://0.0.0.0:8531"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
    volumes:
      - agent3-data:/var/lib/arangodb3

  dbserver1:
    <<: *common
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.my-role=DBSERVER"
      - "--cluster.my-address=tcp://dbserver1:8530"
      - "--server.endpoint=tcp://0.0.0.0:8530"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
    volumes:
      - dbserver1-data:/var/lib/arangodb3

  dbserver2:
    <<: *common
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.my-role=DBSERVER"
      - "--cluster.my-address=tcp://dbserver2:8530"
      - "--server.endpoint=tcp://0.0.0.0:8530"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
    volumes:
      - dbserver2-data:/var/lib/arangodb3

  dbserver3:
    <<: *common
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.my-role=DBSERVER"
      - "--cluster.my-address=tcp://dbserver3:8530"
      - "--server.endpoint=tcp://0.0.0.0:8530"
      - "--rocksdb.encryption-keyfile=/etc/arangodb3/encryption-key"
    volumes:
      - dbserver3-data:/var/lib/arangodb3

  coordinator1:
    <<: *common
    ports:
      - "8529:8529"
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.my-role=COORDINATOR"
      - "--cluster.my-address=tcp://coordinator1:8529"
      - "--server.endpoint=tcp://0.0.0.0:8529"
      - "--audit.output=/var/log/arangodb3/audit.log"

  coordinator2:
    <<: *common
    ports:
      - "8530:8529"
    command:
      - "--cluster.agency-endpoint=tcp://agent1:8531"
      - "--cluster.agency-endpoint=tcp://agent2:8531"
      - "--cluster.agency-endpoint=tcp://agent3:8531"
      - "--cluster.my-role=COORDINATOR"
      - "--cluster.my-address=tcp://coordinator2:8529"
      - "--server.endpoint=tcp://0.0.0.0:8529"
      - "--audit.output=/var/log/arangodb3/audit.log"

volumes:
  agent1-data:
  agent2-data:
  agent3-data:
  dbserver1-data:
  dbserver2-data:
  dbserver3-data:
```

---

## Production Checklist

Use this checklist before deploying to production.

### Encryption

- [ ] Generate a unique 32-byte encryption key: `openssl rand 32 > encryption-key`
- [ ] Set file permissions: `chmod 600 encryption-key`
- [ ] Configure `--rocksdb.encryption-keyfile` on all DB-Servers and Agents
- [ ] Verify encryption: after startup, confirm SST files on disk contain no plaintext (`xxd <sst-file> | head` should show random bytes)
- [ ] Store a backup copy of the encryption key in a secure vault (losing the key = losing all data)
- [ ] Plan key rotation schedule if using `--rocksdb.encryption-keyfolder`

### TLS and Certificates

- [ ] Generate TLS certificates for every node with correct SANs (DNS names and IP addresses)
- [ ] Use a private CA for internal cluster communication
- [ ] Configure `--ssl.keyfile` and `--ssl.cafile` on every node
- [ ] Set `--ssl.protocol 5` (TLS 1.3) for maximum security
- [ ] Enable mTLS: set `--ssl.cafile` to require client certificates
- [ ] Set up certificate rotation before expiry (use `POST /_admin/ssl/reload` for zero-downtime rotation)
- [ ] Verify: `openssl s_client -connect node:8529` shows the correct certificate

### Authentication and Authorization

- [ ] Configure LDAP if using external identity management
- [ ] Test LDAP connectivity independently: `ldapsearch -x -H ldap://server -b "base_dn"`
- [ ] Set `--ldap.refresh-rate` appropriately (lower = faster role updates, higher = less LDAP load)
- [ ] Disable or restrict root user access; use LDAP-mapped roles for daily operations
- [ ] Enable audit logging: `--audit.output` and `--audit.topics`

### Backup Schedule

- [ ] Configure automated hot backup schedule (e.g., every 6 hours)
- [ ] Example cron: `0 */6 * * * curl -X POST http://localhost:8529/_admin/backup/create -H "Authorization: bearer $JWT" -d '{"label":"scheduled"}'`
- [ ] Configure cloud backup upload after each hot backup
- [ ] Test restore procedure: create backup, restore to a test instance, verify data integrity
- [ ] Set backup retention policy: delete backups older than N days
- [ ] Store cloud backup credentials securely (not in plaintext config files)

### Cluster Health

- [ ] Deploy at least 3 Agents for Raft consensus
- [ ] Deploy at least 2 Coordinators behind a load balancer
- [ ] Deploy at least 3 DB-Servers with replication factor >= 2
- [ ] Configure health checks: `GET /_admin/cluster/health`
- [ ] Set up monitoring for replication lag, disk usage, and memory consumption
- [ ] Configure `LimitNOFILE=65536` (or higher) in systemd service files

### DC-to-DC Replication (if applicable)

- [ ] Deploy arangosync master processes (at least 2 per datacenter)
- [ ] Generate and distribute mTLS certificates for inter-datacenter communication
- [ ] Test failover: stop source replication, verify target cluster is usable
- [ ] Monitor replication lag via arangosync status endpoint
- [ ] Document the recovery procedure for split-brain scenarios

### Monitoring and Alerting

- [ ] Export ArangoDB metrics to Prometheus: `GET /_admin/metrics`
- [ ] Ship audit logs to a centralized logging system (ELK, Loki, Splunk)
- [ ] Set alerts for: cluster node down, replication lag > threshold, disk usage > 80%, backup failure
- [ ] Monitor encryption key file integrity (checksum verification)

### Performance Tuning

- [ ] Use SmartGraphs with appropriate `smartGraphAttribute` for graph workloads
- [ ] Use SatelliteCollections for small lookup tables joined with large collections
- [ ] Enable ReadFromFollower for read-heavy workloads: `X-Arango-Allow-Dirty-Read: true`
- [ ] Use `inBackground: true` for index builds on large collections in production
- [ ] Configure `optimizeTopK` on ArangoSearch views for ranked search queries
- [ ] Size the number of shards based on data volume (rule of thumb: 1-10 million docs per shard)

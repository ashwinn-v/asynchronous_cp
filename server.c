#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_log.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

int main(int argc, char *argv[]) {
    int ret;
    uint16_t port_id = 0; // configure port 0

    // Initialize the Environment Abstraction Layer (EAL)
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Error with EAL initialization\n");
        return -1;
    }

    // Create a memory pool for packet buffers
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                                            MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
                                                            rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Cannot create packet buffer pool\n");
        rte_eal_cleanup();
        return -1;
    }

    // NIC Configuration
    struct rte_eth_conf port_conf = {0};

    // Configure the Ethernet device
    ret = rte_eth_dev_configure(port_id, 1, 1, &port_conf);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to configure Ethernet device\n");
        rte_eal_cleanup();
        rte_mempool_free(mbuf_pool);
        return -1;
    }

    // Setup RX queue
    struct rte_eth_rxconf rx_conf;
    memset(&rx_conf, 0, sizeof(rx_conf));
    ret = rte_eth_rx_queue_setup(port_id, 0, RX_RING_SIZE, rte_eth_dev_socket_id(port_id), &rx_conf, mbuf_pool);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to setup RX queue\n");
        rte_eal_cleanup();
        rte_mempool_free(mbuf_pool);
        return -1;
    }

    // Setup TX queue
    struct rte_eth_txconf tx_conf;
    memset(&tx_conf, 0, sizeof(tx_conf));
    ret = rte_eth_tx_queue_setup(port_id, 0, TX_RING_SIZE, rte_eth_dev_socket_id(port_id), &tx_conf);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to setup TX queue\n");
        rte_eal_cleanup();
        rte_mempool_free(mbuf_pool);
        return -1;
    }

    // Start the Ethernet device
    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to start Ethernet device\n");
        rte_eal_cleanup();
        rte_mempool_free(mbuf_pool);
        return -1;
    }

    // Main loop to receive and process packets
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;

    while (1) {
        nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
        for (int i = 0; i < nb_rx; i++) {
            struct rte_mbuf *m = bufs[i];
            printf("Received packet of length %u bytes\n", rte_pktmbuf_pkt_len(m));
            rte_pktmbuf_free(m);
        }
    }

    // Cleanup and exit
    rte_eth_dev_stop(port_id);
    rte_mempool_free(mbuf_pool);
    rte_eal_cleanup();

    return 0;
}

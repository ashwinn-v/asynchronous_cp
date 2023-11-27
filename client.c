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

#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

int main(int argc, char *argv[]) {
    int ret;
    uint16_t port_id = 0; // Configure port 0


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
    ret = rte_eth_dev_configure(port_id, 0, 1, &port_conf);
    if (ret < 0) {
        rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to configure Ethernet device\n");
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

    // Main loop to send packets
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_tx;

    while (1) {
        // Allocate and prepare packets
        for (int i = 0; i < BURST_SIZE; i++) {
            bufs[i] = rte_pktmbuf_alloc(mbuf_pool);
            if (bufs[i] == NULL) {
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to allocate mbuf\n");
                break; // Exit the loop if allocation fails
            }

            // Example packet data
            char *data = "Hello test!";
            size_t data_len = strlen(data);

            // Ensure the packet buffer is large enough
            if (rte_pktmbuf_tailroom(bufs[i]) < data_len) {
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Not enough room in the mbuf; packet too large\n");
                rte_pktmbuf_free(bufs[i]);
                continue;
            }

            // Copy data into the packet buffer
            char *payload = rte_pktmbuf_append(bufs[i], data_len);
            if (payload == NULL) {
                rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Failed to append data to mbuf\n");
                rte_pktmbuf_free(bufs[i]);
                continue;
            }
            memcpy(payload, data, data_len);
        }

        // Send the packets
        nb_tx = rte_eth_tx_burst(port_id, 0, bufs, BURST_SIZE);
        if (nb_tx < BURST_SIZE) {
            rte_log(RTE_LOG_ERR, RTE_LOGTYPE_USER1, "Not all packets sent. Sent %u of %u\n", nb_tx, BURST_SIZE);
        }

        // Free any unsent packets
        for (int i = nb_tx; i < BURST_SIZE; i++) {
            rte_pktmbuf_free(bufs[i]);
        }
    }

    // Cleanup and exit
    rte_eth_dev_stop(port_id);
    rte_mempool_free(mbuf_pool);
    rte_eal_cleanup();

    return 0;
}

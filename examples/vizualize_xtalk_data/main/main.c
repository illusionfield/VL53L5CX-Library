/*******************************************************************************
* Copyright (c) 2020, STMicroelectronics - All Rights Reserved
*
* This file is part of the VL53L5CX Ultra Lite Driver and is dual licensed,
* either 'STMicroelectronics Proprietary license'
* or 'BSD 3-clause "New" or "Revised" License' , at your option.
*
********************************************************************************
*
* 'STMicroelectronics Proprietary license'
*
********************************************************************************
*
* License terms: STMicroelectronics Proprietary in accordance with licensing
* terms at www.st.com/sla0081
*
* STMicroelectronics confidential
* Reproduction and Communication of this document is strictly prohibited unless
* specifically authorized in writing by STMicroelectronics.
*
*
********************************************************************************
*
* Alternatively, the VL53L5CX Ultra Lite Driver may be distributed under the
* terms of 'BSD 3-clause "New" or "Revised" License', in which case the
* following provisions apply instead of the ones mentioned above :
*
********************************************************************************
*
* License terms: BSD 3-clause "New" or "Revised" License.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice, this
* list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
*
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software
* without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
*******************************************************************************/

/***********************************/
/*   VL53L5CX ULD visualize Xtalk  */
/***********************************/
/*
* This example shows the possibility of VL53L5CX to visualize Xtalk data. It
* initializes the VL53L5CX ULD, perform a Xtalk calibration, and starts
* a ranging to capture 10 frames.

* In this example, we also suppose that the number of target per zone is
* set to 1 , and all output are enabled (see file platform.h).
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "vl53l5cx_api.h"
#include "vl53l5cx_plugin_xtalk.h"

void app_main(void)
{

    //Define the i2c bus configuration
    i2c_port_t i2c_port = I2C_NUM_1;
    i2c_master_bus_config_t i2c_mst_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = i2c_port,
            .scl_io_num = 2,
            .sda_io_num = 1,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    //Define the i2c device configuration
    i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = VL53L5CX_DEFAULT_I2C_ADDRESS >> 1,
            .scl_speed_hz = VL53L5CX_MAX_CLK_SPEED,
    };

    /*********************************/
    /*   VL53L5CX ranging variables  */
    /*********************************/

    uint8_t 				status, loop, isAlive, isReady;
    VL53L5CX_Configuration 	Dev;			/* Sensor configuration */
    VL53L5CX_ResultsData 	Results;		/* Results data from VL53L5CX */

    /* Buffer containing Xtalk data */
    uint8_t					xtalk_data[VL53L5CX_XTALK_BUFFER_SIZE];


    /*********************************/
    /*      Customer platform        */
    /*********************************/

    Dev.platform.bus_config = i2c_mst_config;

    //Register the device
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &Dev.platform.handle);

    /* (Optional) Reset sensor */
    Dev.platform.reset_gpio = GPIO_NUM_5;
    VL53L5CX_Reset_Sensor(&(Dev.platform));

    /* (Optional) Set a new I2C address if the wanted address is different
    * from the default one (filled with 0x20 for this example).
    */
    //status = vl53l5cx_set_i2c_address(&Dev, 0x20);


    /*********************************/
    /*   Power on sensor and init    */
    /*********************************/

    /* (Optional) Check if there is a VL53L5CX sensor connected */
    status = vl53l5cx_is_alive(&Dev, &isAlive);
    if(!isAlive || status)
    {
        printf("VL53L5CX not detected at requested address\n");
        return;
    }

    /* (Mandatory) Init VL53L5CX sensor */
    status = vl53l5cx_init(&Dev);
    if(status)
    {
        printf("VL53L5CX ULD Loading failed\n");
        return;
    }

    printf("VL53L5CX ULD ready ! (Version : %s)\n",
           VL53L5CX_API_REVISION);


    /*********************************/
    /*    Start Xtalk calibration    */
    /*********************************/

    /* Start Xtalk calibration with a 3% reflective target at 600mm for the
     * sensor, using 4 samples.
     */

    printf("Running Xtalk calibration...\n");

    status = vl53l5cx_calibrate_xtalk(&Dev, 3, 4, 600);
    if(status)
    {
        printf("vl53l5cx_calibrate_xtalk failed, status %u\n", status);
        return;
    }else
    {
        printf("Xtalk calibration done\n");

        /* Get Xtalk calibration data, in order to use them later */
        status = vl53l5cx_get_caldata_xtalk(&Dev, xtalk_data);

        /* Set Xtalk calibration data */
        status = vl53l5cx_set_caldata_xtalk(&Dev, xtalk_data);
    }

    /* (Optional) Visualize Xtalk grid and Xtalk shape */
    uint32_t i, j;
    union Block_header *bh_ptr;
    uint32_t xtalk_signal_kcps_grid[VL53L5CX_RESOLUTION_8X8];
    uint16_t xtalk_shape_bins[144];

    /* Swap buffer */
    VL53L5CX_SwapBuffer(xtalk_data, VL53L5CX_XTALK_BUFFER_SIZE);

    /* Get data */
    for(i = 0; i < VL53L5CX_XTALK_BUFFER_SIZE; i = i + 4)
    {
        bh_ptr = (union Block_header *)&(xtalk_data[i]);
        if (bh_ptr->idx == 0xA128){
            printf("Xtalk shape bins located at position %#06x\n", (int)i);
            for (j = 0; j < 144; j++){
                memcpy(&(xtalk_shape_bins[j]), &(xtalk_data[i + 4 + j * 2]), 2);
                printf("xtalk_shape_bins[%d] = %d\n", (int)j, (int)xtalk_shape_bins[j]);
            }
        }
        if (bh_ptr->idx == 0x9FFC){
            printf("Xtalk signal kcps located at position %#06x\n", (int)i);
            for (j = 0; j < VL53L5CX_RESOLUTION_8X8; j++){
                memcpy(&(xtalk_signal_kcps_grid[j]), &(xtalk_data[i + 4 + j * 4]), 4);
                xtalk_signal_kcps_grid[j] /= 2048;
                printf("xtalk_signal_kcps_grid[%d] = %d\n", (int)j, (int)xtalk_signal_kcps_grid[j]);
            }
        }
    }

    /* Re-Swap buffer (in case of re-using data later) */
    VL53L5CX_SwapBuffer(xtalk_data, VL53L5CX_XTALK_BUFFER_SIZE);


    /*********************************/
    /*         Ranging loop          */
    /*********************************/

    status = vl53l5cx_start_ranging(&Dev);

    loop = 0;
    while(loop < 10)
    {
        /* Use polling function to know when a new measurement is ready.
         * Another way can be to wait for HW interrupt raised on PIN A1
         * (INT) when a new measurement is ready */

        status = vl53l5cx_check_data_ready(&Dev, &isReady);

        if(isReady)
        {
            vl53l5cx_get_ranging_data(&Dev, &Results);

            /* As the sensor is set in 4x4 mode by default, we have a total
             * of 16 zones to print. For this example, only the data of first zone are
             * print */
            printf("Print data no : %3u\n", Dev.streamcount);
            for(i = 0; i < 16; i++)
            {
                printf("Zone : %3d, Status : %3u, Distance : %4d mm\n",
                       (int)i,
                       Results.target_status[VL53L5CX_NB_TARGET_PER_ZONE*i],
                       Results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE*i]);
            }
            printf("\n");
            loop++;
        }

        /* Wait a few ms to avoid too high polling (function in platform
         * file, not in API) */
        VL53L5CX_WaitMs(&(Dev.platform), 5);
    }

    status = vl53l5cx_stop_ranging(&Dev);
    printf("End of ULD demo\n");
}

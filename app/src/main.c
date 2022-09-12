/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/device.h>

#include "app_version.h"

#include <zephyr/logging/log.h>
#include <nrfx_saadc.h>
#include <nrfx_ppi.h>
#include <hal/nrf_gpio.h>
LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

nrf_saadc_value_t value[100];

static uint32_t stamp;
static uint8_t ppi_ch;
static void adc_event_handler(nrfx_saadc_evt_t const *event)
{
	LOG_INF("type:%d", event->type);
	if (event->type == NRFX_SAADC_EVT_DONE) {
		stamp = NRF_RTC1->COUNTER - stamp;
		LOG_INF("took %d cycles", stamp);
		for (int i = 0; i < 1; i++) {
			LOG_INF("val: %d", value[i]);
		}
	}
}

static int adc_init(void)
{
	nrfx_err_t err;
	nrfx_saadc_channel_t channel =
		NRFX_SAADC_DEFAULT_CHANNEL_DIFFERENTIAL(NRF_SAADC_INPUT_AIN6,
							NRF_SAADC_INPUT_AIN7, 0);
	nrfx_saadc_adv_config_t adv_config = NRFX_SAADC_DEFAULT_ADV_CONFIG;
//	adv_config.oversampling = NRF_SAADC_OVERSAMPLE_8X;
	/*channel.channel_config.acq_time = NRF_SAADC_ACQTIME_15US;*/

	nrf_gpio_cfg_input(29, NRF_GPIO_PIN_NOPULL);
	nrf_gpio_cfg_input(30, NRF_GPIO_PIN_NOPULL);
	nrf_gpio_cfg_input(31, NRF_GPIO_PIN_NOPULL);

	nrfx_saadc_init(7);
	NRFX_IRQ_ENABLE(SAADC_IRQn);
	IRQ_CONNECT(SAADC_IRQn, 3, nrfx_saadc_irq_handler, 0, 0);

	err = nrfx_saadc_channel_config(&channel);
	if (err != NRFX_SUCCESS) {
		return -EIO;
	}

	err = nrfx_saadc_advanced_mode_set(BIT(0), NRF_SAADC_RESOLUTION_14BIT,
			&adv_config, adc_event_handler);
	if (err != NRFX_SUCCESS) {
		return -EIO;
	}

	err = nrfx_ppi_channel_alloc(&ppi_ch);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Channel alloc failed: %08x", err);
		return -EIO;
	}

	err = nrfx_ppi_channel_assign(ppi_ch,
			nrf_saadc_event_address_get(NRF_SAADC, NRF_SAADC_EVENT_DONE),
			nrf_saadc_task_address_get(NRF_SAADC, NRF_SAADC_TASK_SAMPLE));
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Channel assigning failed: %08x", err);
		return -EIO;
	}

	err = nrfx_ppi_channel_enable(ppi_ch);
	if (err != NRFX_SUCCESS) {
		LOG_ERR("Channel enable failed: %08x", err);
		return -EIO;
	}
	return 0;
}

static int adc_buffer_set(void)
{
	nrfx_err_t err;

	err = nrfx_saadc_buffer_set(value, 1);//ARRAY_SIZE(value));
	if (err != NRFX_SUCCESS) {
		return -EIO;
	}

	return 0;
}

static int adc_trigger(void)
{
	nrfx_err_t err;

	LOG_INF("Triggering...");
	err = nrfx_saadc_mode_trigger();
	if (err != NRFX_SUCCESS) {
		return -EIO;
	}

	return 0;
}
void main(void)
{
	int err;

	printk("Zephyr Example Application %s\n", APP_VERSION_STR);

	err = adc_init();
	if (err < 0) {
		LOG_ERR("init failed");
		return;
	}

	err = adc_buffer_set();
	if (err < 0) {
		LOG_ERR("init failed");
		return;
	}

	stamp = NRF_RTC1->COUNTER;

	err = adc_trigger();
	if (err < 0) {
		LOG_ERR("init failed");
		return;
	}

	k_msleep(100);
}


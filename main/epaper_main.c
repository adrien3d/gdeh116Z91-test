/* SPI Master example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "epd.h"

//#ifdef CONFIG_IDF_TARGET_ESP32
#define LCD_HOST    HSPI_HOST
#define DMA_CHAN    2
/*#elif defined CONFIG_IDF_TARGET_ESP32S2
#define LCD_HOST    SPI2_HOST
#define DMA_CHAN    LCD_HOST
#endif*/

// Waveshare ESP32 ePaper
/*#define EPD_PIN_CLK     13
#define EPD_PIN_MOSI    14
#define EPD_PIN_CS      15
#define EPD_PIN_DC      27
#define EPD_PIN_RST     26
#define EPD_PIN_BUSY    25*/

// Atc1441 config
/*#define EPD_PIN_CLK     15
#define EPD_PIN_MOSI    23
#define EPD_PIN_CS      5
#define EPD_PIN_DC      17
#define EPD_PIN_RST     16
#define EPD_PIN_BUSY    4*/

// Tester ESP32
#define EPD_PIN_MOSI    23
#define EPD_PIN_CLK     18
#define EPD_PIN_CS      32
#define EPD_PIN_DC      27
#define EPD_PIN_RST     26
#define EPD_PIN_BUSY    35

// One side
/*#define EPD_PIN_MOSI    23
#define EPD_PIN_CLK     18
#define EPD_PIN_CS      5
#define EPD_PIN_DC      16
#define EPD_PIN_RST     4
#define EPD_PIN_BUSY    15*/

#define DELAY_11_6      100

#define SCREEN_TOTAL_BYTES 76800

static const char *TAG = "epaper";
spi_device_handle_t spi;

void epd_cmd(const uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&cmd;               //The data is the cmd itself
    t.user=(void*)0;                //D/C needs to be set to 0
    gpio_set_level(EPD_PIN_DC, 0);
    gpio_set_level(EPD_PIN_CS, 0);
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    gpio_set_level(EPD_PIN_CS, 1);
    //gpio_set_level(EPD_PIN_DC, 1);
    //assert(ret==ESP_OK);            //Should have had no issues.
}

void epd_data(const uint8_t data) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=8;                     //Command is 8 bits
    t.tx_buffer=&data;               //The data is the cmd itself
    t.user=(void*)1;                //D/C needs to be set to 1
    gpio_set_level(EPD_PIN_DC, 1);
    gpio_set_level(EPD_PIN_CS, 0);
    ret=spi_device_polling_transmit(spi, &t);  //Transmit!
    gpio_set_level(EPD_PIN_CS, 1);
    //assert(ret==ESP_OK);            //Should have had no issues.
}

/*
//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}
*/

void _waitBusy(const char *message)
{
  ESP_LOGI(TAG, "_waitBusy for %s", message);
  int64_t time_since_waitBusy_call = esp_timer_get_time();

  while (1)
  {
    if (gpio_get_level((gpio_num_t)EPD_PIN_BUSY) == 0)
      break;
    vTaskDelay(1);
    if (esp_timer_get_time() - time_since_waitBusy_call > 40000000)
    {
      ESP_LOGI(TAG, "Busy Timeout");
      break;
    }
  }
}

void EPD_HW_Init(void)
{
    gpio_set_direction((gpio_num_t)EPD_PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)EPD_PIN_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)EPD_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)EPD_PIN_BUSY, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)EPD_PIN_BUSY, GPIO_PULLUP_ONLY);

    gpio_set_level(EPD_PIN_RST, 0);
    vTaskDelay(DELAY_11_6 / portTICK_PERIOD_MS);
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(DELAY_11_6 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Step 1 done: Power on");

    epd_cmd(0x12); //Software Reset
    _waitBusy("0x12 Software reset");
    vTaskDelay(10 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Step 2 done: Resets");

    epd_cmd(0x0C);  // Soft start setting
    epd_data(0xAE);
    epd_data(0xC7);
    epd_data(0xC3);
    epd_data(0xC0);
    epd_data(0x40);

    epd_cmd(0x01);  // Set MUX as 639
    epd_data(0x7F);
    epd_data(0x02);
    epd_data(0x00);

    epd_cmd(0x11);  // Data entry mode
    epd_data(0x02);

    epd_cmd(0x44);
    epd_data(0x00); // RAM x address start at 0
    epd_data(0x00);
    epd_data(0xBF); // RAM x address end at 3BFh -> 959
    epd_data(0x03);
    epd_cmd(0x45);
    epd_data(0x7F); // RAM y address start at 27Fh;
    epd_data(0x02);
    epd_data(0x00); // RAM y address end at 00h;
    epd_data(0x00);

    epd_cmd(0x3C); // VBD
    epd_data(0x01); // LUT1, for white
    ESP_LOGI(TAG, "Step 3 done: Initialization");

    epd_cmd(0x18);
    epd_data(0x80);
    epd_cmd(0x22);
    epd_data(0xB1);
    epd_cmd(0x20);
    _waitBusy("load waveform");
    vTaskDelay(10 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Step 4 done: Load waveform LUT");

    epd_cmd(0x4E);//Ram window set
    epd_data(0x00);
    epd_data(0x00);
    epd_cmd(0x4F);
    epd_data(0x7F);
    epd_data(0x02);
}

void EPD_Update(void)
{
    epd_cmd(0x22); //Display Update Control
    epd_data(0xF7);   
    epd_cmd(0x20);  //Activate Display Update Sequence
    _waitBusy("epd update");   
}

void EPD_WhiteScreen_ALL(const unsigned char *BW_datas,const unsigned char *R_datas)
{
    unsigned int i;  
    epd_cmd(0x24);   //write RAM for black(0)/white (1)
    for( i= 0; i < SCREEN_TOTAL_BYTES; i++)
    {               
        epd_data(BW_datas[i]);
    }
    epd_cmd(0x26);   //write RAM for black(0)/white (1)
    for(i = 0; i < SCREEN_TOTAL_BYTES; i++)
    {               
        epd_data(~R_datas[i]);
    }
    EPD_Update();
}

void EPD_WhiteScreen_ALL_Clean(void)
{
    unsigned int i;  
    epd_cmd(0x24);   //write RAM for black(0)/white (1)
    for( i= 0; i < SCREEN_TOTAL_BYTES; i++)
    {               
        epd_data(0xff);
    }
    epd_cmd(0x26);   //write RAM for black(0)/white (1)
    for(i = 0; i < SCREEN_TOTAL_BYTES; i++)
    {               
        epd_data(0x00);
    }
    EPD_Update();
}

void EPD_DeepSleep(void)
{
    epd_cmd(0x10); //enter deep sleep
    epd_data(0x01); 
    vTaskDelay(DELAY_11_6 / portTICK_PERIOD_MS);
}

void app_main(void)
{
    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=-1,
        .mosi_io_num=EPD_PIN_MOSI,
        .sclk_io_num=EPD_PIN_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=4094
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=4000000,
        .mode=0,                                //SPI mode 0
        .spics_io_num=EPD_PIN_CS,               //CS pin
        .queue_size=7,                          //We want to be able to queue 7 transactions at a time
        //.pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(LCD_HOST, &buscfg, DMA_CHAN);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(LCD_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    //Initialize the LCD

    while (1) {
        //Full screen refresh
        EPD_HW_Init(); //Electronic paper initialization
        EPD_WhiteScreen_ALL(gImage_BW, gImage_R); //Refresh the picture in full screen
        EPD_DeepSleep(); //Enter deep sleep,Sleep instruction is necessary, please do not delete!!! 
        ESP_LOGI(TAG, "Display image");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        
        //Clean
        EPD_HW_Init(); //Electronic paper initialization
        EPD_WhiteScreen_ALL_Clean();
        EPD_DeepSleep(); //Enter deep sleep,Sleep instruction is necessary, please do not delete!!!
        ESP_LOGI(TAG, "Cleaned");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
    //epd_init();
    //Initialize the effect displayed
}

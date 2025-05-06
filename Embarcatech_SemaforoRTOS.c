#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "pico/bootrom.h"
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "lib/ws2812.h"
#include "lib/font.h"
#include "lib/ssd1306.h"


#define I2C_PORT i2c1   //definição do canal
#define I2C_SDA 14      //pino de dados
#define I2C_SCL 15      //pino de clock
#define address 0x3C    //endereço do canal

#define ledR 13
#define ledG 11
#define ledB 12

#define buttonB 6
#define buttonA 5

bool volatile flag = false; //flag do modo noturno
uint64_t last_time = 0; //variavel de tempo para auxiliar no debouncing 

bool color = true;  //variavel que indica que se o pixel está ligado ou desligado
ssd1306_t ssd; //inicializa a estrutura do display

void vRGB1Task()
{
    //inicialização do led rgb
    gpio_init(ledR);
    gpio_init(ledG);
    gpio_init(ledB);

    gpio_set_dir(ledR, GPIO_OUT);
    gpio_set_dir(ledG, GPIO_OUT);
    gpio_set_dir(ledB, GPIO_OUT);

    while(1)
    {
        if(!flag) // modo noturno desativado
        {
            gpio_put(ledG, true);  //acende o led verde
            vTaskDelay(pdMS_TO_TICKS(5000)); //verde fica 5 segundos acesso
            gpio_put(ledR, true);   //acende o led vermelho para formar o amarelo
            vTaskDelay(pdMS_TO_TICKS(3000)); //amarelo fica 3 segundos acesso
            gpio_put(ledG, false); //desliga o verde e deixa o vermelho acesso
            vTaskDelay(pdMS_TO_TICKS(5000)); //vermelho fica 5 segundos acesso  
            gpio_put(ledR, false);  //desliga o led vermelho
        }
        else
        {
            gpio_put(ledG, true);  //acende o led verde
            gpio_put(ledR, true);   //acende o led vermelho 
            vTaskDelay(pdMS_TO_TICKS(1000)); //amarelo fica 1 segundo acesso
            gpio_put(ledR, false);   //desliga o led vermelho 
            gpio_put(ledG, false);  //desliga o led verde
            vTaskDelay(pdMS_TO_TICKS(500)); //amarelo fica 0,5 segundos apagado
        }
             
    }
}

void vMatrix2Task()
{
    npInit(LED_PIN);        //inicializa matriz de led
    npClear();              //limpa a matriz

    int frame[5][5] = {
        {0, 0, 0, 0, 0},
        {0, 1, 1, 1, 0},
        {0, 1, 1, 1, 0},
        {0, 1, 1, 1, 0},
        {0, 0, 0, 0, 0}
        };

    while(1)
    {
        if(!flag)   //modo noturno desativado
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds
            print_frame(frame, 0, 40, 0); //acende os leds verdes na matriz
            vTaskDelay(pdMS_TO_TICKS(5000)); //verde fica 5 segundos acesso
            print_frame(frame, 40, 40, 0); //acende os leds amarelos na matriz
            vTaskDelay(pdMS_TO_TICKS(3000)); //amarelo fica 3 segundos acesso
            print_frame(frame, 40, 0, 0);  //acende os leds vermelhos na matriz
            vTaskDelay(pdMS_TO_TICKS(5000)); //vermelho fica 5 segundos acesso  
            npClear();  //desliga os leds da matriz
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds
            print_frame(frame, 40, 40, 0); //acende os leds verdes na matriz
            vTaskDelay(pdMS_TO_TICKS(1000)); //amarelo fica 1 segundo acesso
            npClear();
            vTaskDelay(pdMS_TO_TICKS(500)); //amarelo fica 0,5 segundos apagado
        }
    }
}



void vDisplay3Task()
{
    init_display();

    while(1)
    {
        if(!flag)
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Verde", 44,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(4900)); //verde fica 5 segundos acesso
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Amarelo", 36,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(2900)); //amarelo fica 3 segundos acesso
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Vermelho", 32,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(4900)); //vermelho fica 5 segundos acesso  
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Amarelo", 36,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(900)); //amarelo fica 1 segundo acesso
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(400)); //amarelo fica 0,5 segundo apagado
        }
    }
}

void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint64_t current_time = to_ms_since_boot(get_absolute_time()); 
    
    if(current_time - last_time > 200)
    {
        if(gpio == buttonB) 
        {
            npClear();
            ssd1306_fill(&ssd, !color); // Limpa o display
            ssd1306_send_data(&ssd);  // Atualiza o display
            reset_usb_boot(0, 0);
        }
        else if(gpio == buttonA)
        {
            flag = !flag;
        }  
        last_time = current_time; 
    }
}

void init_buttons()
{
    gpio_init(buttonA);
    gpio_init(buttonB);

    gpio_set_dir(buttonA, GPIO_IN);
    gpio_set_dir(buttonB, GPIO_IN);

    gpio_pull_up(buttonA);
    gpio_pull_up(buttonB);
}

void init_display()
{
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);  //seta o pino gpio como i2c
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);  //seta o pino gpio como i2c
    gpio_pull_up(I2C_SDA);  //ativa o resistor de pull up para gantir o nível lógico alto
    gpio_pull_up(I2C_SCL);  //ativa o resistor de pull up para gantir o nível lógico alto


    ssd1306_init(&ssd, WIDTH, HEIGHT, false, address, I2C_PORT); //inicializa o display
    ssd1306_config(&ssd); //configura o display
    ssd1306_send_data(&ssd); //envia os dados para o display
  
    //limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

int main()
{
    stdio_init_all();

    init_buttons();
    gpio_set_irq_enabled_with_callback(buttonB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(buttonA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);


    xTaskCreate(vRGB1Task, "RGB Task 1", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vMatrix2Task, "Matrix Task 2", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vDisplay3Task, "Display Task 3", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();

    // while (true) {
    //     printf("Hello, world!\n");
    //     sleep_ms(1000);
    // }
}

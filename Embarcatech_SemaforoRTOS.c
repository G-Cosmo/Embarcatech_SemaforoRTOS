#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
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

//pinos do led rgb
#define ledR 13
#define ledG 11
#define ledB 12

//pinos dos botões
#define buttonB 6
#define buttonA 5

#define BUZZER_PIN 10   //pino do buzzer

bool volatile flag = false; //flag do modo noturno
uint64_t last_time = 0; //variavel de tempo para auxiliar no debouncing 

bool color = true;  //variavel que indica que se o pixel está ligado ou desligado
ssd1306_t ssd; //inicializa a estrutura do display

void init_buttons()
{
    //pinos dos botões
    gpio_init(buttonA); 
    gpio_init(buttonB); 

    //direção dos botões
    gpio_set_dir(buttonA, GPIO_IN); 
    gpio_set_dir(buttonB, GPIO_IN);

    //ativação dos resistores de pull-up
    gpio_pull_up(buttonA);
    gpio_pull_up(buttonB);
}

void init_display()
{
    i2c_init(I2C_PORT, 400*1000); //definição da porta e frequencia 
    
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

uint init_pwm(uint gpio, uint wrap) {

    gpio_set_function(gpio, GPIO_FUNC_PWM); //define o pino do argumento como PWM

    uint slice_num = pwm_gpio_to_slice_num(gpio);   //armazena a slice do PWM
    pwm_set_wrap(slice_num, wrap);  //configura o wrap do PWM (máximo do contador)
    
    pwm_set_enabled(slice_num, true);  //ativa o PWM
    return slice_num;   //retorna a slice do PWM
}

void play_buzzer(uint freq, float duty_cycle) {

    uint slice = pwm_gpio_to_slice_num(BUZZER_PIN); //armazena a slice do PWM do buzzer
    uint clock_divider = 4; // Define o divisor do clock (ajuste se necessário)
    uint wrap = clock_get_hz(clk_sys) / (clock_divider * freq); //calcula o wrap do PWM buzzer

    pwm_set_clkdiv(slice, clock_divider);   //configura o divisor de clock
    pwm_set_wrap(slice, wrap);  //configura o wrap
    pwm_set_gpio_level(BUZZER_PIN, wrap * duty_cycle);  //ativa o pwm
}

void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint64_t current_time = to_ms_since_boot(get_absolute_time()); //armazena o registrado no momento da interrupção
    
    if(current_time - last_time > 200)  //checa se passou pelo menos 200 ms entre duas ativações do botão
    {
        if(gpio == buttonB) 
        {
            npClear();  //limpa a matriz de leds
            ssd1306_fill(&ssd, !color); // Limpa o display
            ssd1306_send_data(&ssd);  // Atualiza o display
            reset_usb_boot(0, 0);   //coloca em modo bootloader
        }
        else if(gpio == buttonA)
        {
            flag = !flag;   //alterna o estado da flag
        }  
        last_time = current_time; //atualiza as variáveis de tempo
    }
}

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
            if(flag) continue;  //checa se a flag foi alterada
            gpio_put(ledR, true);   //acende o led vermelho para formar o amarelo
            vTaskDelay(pdMS_TO_TICKS(3000)); //amarelo fica 3 segundos acesso
            if(flag) continue;  //checa se a flag foi alterada
            gpio_put(ledG, false); //desliga o verde e deixa o vermelho acesso
            vTaskDelay(pdMS_TO_TICKS(5000)); //vermelho fica 5 segundos acesso  
            if(flag) continue;  //checa se a flag foi alterada
            gpio_put(ledR, false);  //desliga o led vermelho
        }
        else
        {
            gpio_put(ledG, true);  //acende o led verde
            gpio_put(ledR, true);   //acende o led vermelho 
            vTaskDelay(pdMS_TO_TICKS(1500)); //amarelo fica 1,5 segundos acesso
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

    //define quais leds serão acessos
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
            if(flag) continue; //checa se a flag foi alterada
            print_frame(frame, 40, 40, 0); //acende os leds amarelos na matriz
            vTaskDelay(pdMS_TO_TICKS(3000)); //amarelo fica 3 segundos acesso
            if(flag) continue; //checa se a flag foi alterada
            print_frame(frame, 40, 0, 0);  //acende os leds vermelhos na matriz
            vTaskDelay(pdMS_TO_TICKS(5000)); //vermelho fica 5 segundos acesso  
            npClear();  //desliga os leds da matriz

        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds
            print_frame(frame, 40, 40, 0); //acende os leds verdes na matriz
            vTaskDelay(pdMS_TO_TICKS(1500)); //amarelo fica 1,5 segundo acesso
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
            ssd1306_draw_string(&ssd, "Passe", 44,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(4980)); //verde fica 5 segundos acesso
            if(flag) continue; //checa se a flag foi alterada
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Atencao", 36,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(2980)); //amarelo fica 3 segundos acesso
            if(flag) continue; //checa se a flag foi alterada
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Pare", 48,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(4980)); //vermelho fica 5 segundos acesso  
            if(flag) continue; //checa se a flag foi alterada
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_rect(&ssd, 3, 3, 122, 60, color, !color); // Desenha um retângulo
            ssd1306_draw_string(&ssd, "Atencao", 36,28);
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(1470)); //amarelo fica 1 segundo acesso
            ssd1306_fill(&ssd, !color);  // Limpa o display
            ssd1306_send_data(&ssd);  // Atualiza o display
            vTaskDelay(pdMS_TO_TICKS(470)); //amarelo fica 0,5 segundo apagado
        }
    }
}

void vBuzzer4Task()
{
    uint wrap_buzzer = 10000;   //define o wrap do buzzer

    uint pwm_buzzer = init_pwm(BUZZER_PIN, wrap_buzzer);// inicializa o pwm do buzzer
    uint buzzer_freq = 1000;    //frequencia do buzzer

    while(1)
    {
        if(!flag)
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds

            for(int i=0; i<5; i++)  //laço com 5 seg de duração
            {
                play_buzzer(500, 0.5); //bip em 500Hz com 50% de D.C.
                vTaskDelay(pdMS_TO_TICKS(800)); // Toca durante 800 ms
                play_buzzer(500, 0); //bip em 500Hz com 0% de D.C. (desligado)
                vTaskDelay(pdMS_TO_TICKS(200)); // Passa 200 ms desligado
            }

            if(flag) continue; //checa se a flag foi alterada

            for(int i=0; i<15; i++)  //laço com 3 seg de duração
            {
                play_buzzer(5000, 0.5); //bip em 5KHz com 50% de D.C.
                vTaskDelay(pdMS_TO_TICKS(100)); // toca por 100 ms
                play_buzzer(5000, 0); //bip em 5KHz com 0% de D.C. (desligado)
                vTaskDelay(pdMS_TO_TICKS(100)); // desliga por 100 ms
            }

            if(flag) continue; //checa se a flag foi alterada

            for(int i=0; i<5; i++)  //laço com 5 seg de duração
            {
                play_buzzer(2500, 0.5); //bip em 2.5KHz com 50% de D.C.
                vTaskDelay(pdMS_TO_TICKS(300)); // toca por 300 ms
                play_buzzer(2500, 0); //bip em 2.5KHz com 0% de D.C. (desligado)
                vTaskDelay(pdMS_TO_TICKS(700)); // desliga por 700 ms
            }


        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(5)); // delay para sincronizar os leds

            play_buzzer(1000, 0.5); //bip em 1KHz com 50% de D.C.
            vTaskDelay(pdMS_TO_TICKS(1500)); // toca por 1.5 seg
            play_buzzer(1000, 0); //bip em 1KHz com 0% de D.C. (desligado)
            vTaskDelay(pdMS_TO_TICKS(500)); // desliga por 0,5 seg
        }
    }
}

int main()
{
    stdio_init_all();   //inicializa a biblioteca stdio

    init_buttons(); //inicializa os botões
    gpio_set_irq_enabled_with_callback(buttonB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);   // ativa a interrupção para o botão B
    gpio_set_irq_enabled_with_callback(buttonA, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);   // ativa a interrupção para o botão B


    xTaskCreate(vRGB1Task, "RGB Task 1", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);   //cria task do led rgb
    xTaskCreate(vMatrix2Task, "Matrix Task 2", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);   //cria task da matriz de leds
    xTaskCreate(vDisplay3Task, "Display Task 3", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);   //cria task do display
    xTaskCreate(vBuzzer4Task, "Buzzer Task 4", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);   //cria task do buzzer

    vTaskStartScheduler();  //chama o escalonador de tasks
    panic_unsupported();    //trata erros


}

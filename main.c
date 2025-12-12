#include "project.h"
#include "keypad.h"
#include "math.h"
#include <string.h>
#include <stdbool.h>

#define JUMP_DURATION     250
#define BASE_SERVO_POS    2600
#define SERVO_PRESS       2900
#define PI                3.14159265f
#define N                 100

// thresholds
#define PHOTO_DOWN_THRESHOLD 1000
#define PHOTO_UP_THRESHOLD    800

// Globals
float sin_wave[N];
float cos_wave[N];

int score = 40;
bool game_running = false;

char msg[8];                 // bigger and safer buffer
uint8 msg_idx = 0;

// -----------------------------------------------------
// Utility
// -----------------------------------------------------
static inline void toggle_leds(uint8 led1, uint8 led2)
{
    led1_Write(~led1_Read());
    led2_Write(~led2_Read());
}

static void generate_wave(float *buffer, bool sine)
{
    for (int j = 0; j < N; j++)
    {
        float angle = 2 * PI * j / N;
        buffer[j] = sine ? sinf(angle) : cosf(angle);
    }
}

// -----------------------------------------------------
// Sounds
// -----------------------------------------------------
static void play_sound(const float *wave, int duration_ms)
{
    for (int i = 0; i < N; i++)
    {
        VDAC_SetValue(128 + (int)(128 * wave[i]));
        CyDelay(duration_ms / N);
    }
    VDAC_SetValue(0);
}

// -----------------------------------------------------
// Actions
// -----------------------------------------------------
static void jump()
{
    LCD_ClearDisplay();
    LCD_Position(0, 0);
    LCD_PrintString("Jump");

    game_running = true;
    UART_PutString("jump\n");

    toggle_leds(LED_1, LED_2);
    PWM_WriteCompare1(SERVO_PRESS);

    play_sound(sin_wave, JUMP_DURATION);

    PWM_WriteCompare1(BASE_SERVO_POS);
    toggle_leds(LED_1, LED_2);
    LCD_ClearDisplay();
}

static void crouch()
{
    LCD_ClearDisplay();
    LCD_Position(0, 0);
    LCD_PrintString("Crouch");

    UART_PutString("crouch\n");

    toggle_leds(LED_3, LED_4);
    PWM_WriteCompare2(SERVO_PRESS);

    play_sound(cos_wave, JUMP_DURATION * 2);

    PWM_WriteCompare2(BASE_SERVO_POS);
    toggle_leds(LED_3, LED_4);
    LCD_ClearDisplay();
}

// -----------------------------------------------------
// Initialization
// -----------------------------------------------------
static void system_init()
{
    ADC_Start();
    LCD_Start();
    PWM_Start();
    MUX_Start();
    Timer_Start();
    VDAC_Start();
    UART_Start();
    keypadInit();

    VDAC_SetValue(0);

    PWM_WriteCompare1(BASE_SERVO_POS);
    PWM_WriteCompare2(BASE_SERVO_POS);

    // Precompute sound waves
    generate_wave(sin_wave, true);
    generate_wave(cos_wave, false);
}

// -----------------------------------------------------
// Interrupts
// -----------------------------------------------------
CY_ISR(update_score)
{
    if (game_running)
    {
        score += 10;
        LCD_ClearDisplay();
        LCD_Position(1, 0);
        LCD_PrintNumber(score);
    }
    Timer_ReadStatusRegister();
}

CY_ISR(on_RX)
{
    uint8 c = UART_GetChar();
    if (!c) return;

    // CR means command done
    if (c == '\r')
    {
        msg[msg_idx] = '\0';

        if (strcmp(msg, "jump") == 0)
            jump();
        else if (strcmp(msg, "crouch") == 0)
            crouch();

        msg_idx = 0;
        return;
    }

    // Add char if space exists
    if (msg_idx < sizeof(msg)-1)
        msg[msg_idx++] = c;
    else
        msg_idx = 0;    // reset on overflow
}

// -----------------------------------------------------
// Main
// -----------------------------------------------------
int main(void)
{
    CyGlobalIntEnable;

    isr_RX_StartEx(on_RX);
    isr_StartEx(update_score);

    system_init();

    int photo_down, photo_up;
    char key;

    for (;;)
    {
        key = keypadScan();

        if (SW_1_Read() || key == '*')     jump();
        if (SW_2_Read() || key == '#')     crouch();

        if (SW_3_Read())
        {
            game_running = false;
            score = 40;
        }

        // Read photo DOWN
        MUX_FastSelect(0);
        ADC_StartConvert();
        if (ADC_IsEndConversion(ADC_WAIT_FOR_RESULT))
            photo_down = ADC_GetResult16();
        ADC_StopConvert();

        // Read photo UP
        MUX_FastSelect(1);
        ADC_StartConvert();
        if (ADC_IsEndConversion(ADC_WAIT_FOR_RESULT))
            photo_up = ADC_GetResult16();
        ADC_StopConvert();

        // Use thresholds here if needed
        // if (photo_down < PHOTO_DOWN_THRESHOLD) jump();
        // if (photo_up   < PHOTO_UP_THRESHOLD)   crouch();
    }
}

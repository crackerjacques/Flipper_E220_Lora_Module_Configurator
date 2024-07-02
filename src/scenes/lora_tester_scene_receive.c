#include "../lora_tester_app_i.h"
#include <furi_hal_serial.h>
#include <gui/elements.h>

#define UART_CH (FuriHalSerialIdUsart)
#define MAX_BUFFER_SIZE 256

static FuriHalSerialHandle* serial_handle = NULL;

static bool init_receive_context(LoraTesterApp* app);
static void cleanup_receive_context(LoraTesterApp* app);
static void cleanup_uart();
static void reset_text_box(LoraTesterApp* app);

static uint32_t get_current_baud_rate(LoraTesterApp* app) {
    return app->baud_rate;
}

static void
    uart_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    LoraTesterApp* app = (LoraTesterApp*)context;
    if(app && app->receive_context) {
        ReceiveContext* receive_context = app->receive_context;

        if(event == FuriHalSerialRxEventData) {
            uint8_t data = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(receive_context->rx_stream, &data, 1, 0);
            furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerEventRx);
        }
    }
}

static int32_t uart_worker(void* context) {
    LoraTesterApp* app = (LoraTesterApp*)context;
    ReceiveContext* receive_context = app->receive_context;
    uint8_t data[MAX_BUFFER_SIZE];
    size_t length = 0;
    FuriString* new_str = furi_string_alloc();

    while(1) {
        uint32_t events =
            furi_thread_flags_wait(WORKER_ALL_EVENTS, FuriFlagWaitAny, FuriWaitForever);

        if(events & FuriFlagError) {
            FURI_LOG_E("LoRaTester", "Worker thread error");
            break;
        }

        if(events & WorkerEventStop) {
            FURI_LOG_I("LoRaTester", "Worker thread stop event received");
            break;
        }

        if(events & WorkerEventRx) {
            length =
                furi_stream_buffer_receive(receive_context->rx_stream, data, MAX_BUFFER_SIZE, 0);
            if(length > 0) {
                furi_mutex_acquire(receive_context->mutex, FuriWaitForever);

                for(size_t i = 0; i < length; i++) {
                    furi_string_cat_printf(new_str, "%02X", data[i]);
                    furi_string_push_back(new_str, ' ');
                }

                size_t new_text_length =
                    furi_string_size(app->text_box_store) + furi_string_size(new_str);
                if(new_text_length >= LORA_TESTER_TEXT_BOX_STORE_SIZE - 1) {
                    size_t remove_length = new_text_length - LORA_TESTER_TEXT_BOX_STORE_SIZE + 1;
                    furi_string_right(app->text_box_store, remove_length);
                }

                furi_string_cat(app->text_box_store, new_str);
                furi_string_reset(new_str);

                furi_mutex_release(receive_context->mutex);

                view_dispatcher_send_custom_event(
                    app->view_dispatcher, LoraTesterCustomEventRefreshView);
            }
        }
    }

    furi_string_free(new_str);
    return 0;
}

static bool init_receive_context(LoraTesterApp* app) {
    FURI_LOG_D("LoRaTester", "Initializing receive context");
    cleanup_receive_context(app);

    app->receive_context = malloc(sizeof(ReceiveContext));
    if(!app->receive_context) {
        FURI_LOG_E("LoRaTester", "Failed to allocate receive context");
        return false;
    }

    ReceiveContext* context = app->receive_context;
    context->rx_stream = furi_stream_buffer_alloc(MAX_BUFFER_SIZE, 1);
    context->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    if(!context->rx_stream || !context->mutex) {
        FURI_LOG_E("LoRaTester", "Failed to allocate receive context resources");
        cleanup_receive_context(app);
        return false;
    }

    FURI_LOG_D("LoRaTester", "Receive context initialized successfully");
    return true;
}

static void cleanup_receive_context(LoraTesterApp* app) {
    FURI_LOG_D("LoRaTester", "Cleaning up receive context");
    if(app->receive_context) {
        ReceiveContext* context = app->receive_context;
        if(context->rx_stream) {
            furi_stream_buffer_free(context->rx_stream);
            context->rx_stream = NULL;
        }
        if(context->mutex) {
            furi_mutex_free(context->mutex);
            context->mutex = NULL;
        }
        free(context);
        app->receive_context = NULL;
    }
}

static void cleanup_uart() {
    FURI_LOG_D("LoRaTester", "Cleaning up UART");
    if(serial_handle) {
        furi_hal_serial_async_rx_stop(serial_handle);
        furi_hal_serial_deinit(serial_handle);
        furi_hal_serial_control_release(serial_handle);
        serial_handle = NULL;
    }
}

static void reset_text_box(LoraTesterApp* app) {
    FURI_LOG_D("LoRaTester", "Resetting text box");
    furi_assert(app);
    furi_assert(app->text_box);
    furi_assert(app->text_box_store);

    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
    furi_string_printf(app->text_box_store, "Waiting for data...\n");
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
}

void lora_tester_scene_receive_on_enter(void* context) {
    FURI_LOG_I("LoRaTester", "Entering receive scene");
    LoraTesterApp* app = context;

    reset_text_box(app);

    if(!init_receive_context(app)) {
        FURI_LOG_E("LoRaTester", "Failed to initialize receive context");
        return;
    }

    const uint8_t max_retries = 5;
    for(uint8_t retry = 0; retry < max_retries; retry++) {
        FURI_LOG_D("LoRaTester", "Attempting to acquire serial handle (attempt %d)", retry + 1);
        serial_handle = furi_hal_serial_control_acquire(UART_CH);
        if(serial_handle) {
            FURI_LOG_D("LoRaTester", "Successfully acquired serial handle");
            break;
        }
        FURI_LOG_W("LoRaTester", "Failed to acquire serial handle, retrying...");
        furi_delay_ms(100);
    }

    if(!serial_handle) {
        FURI_LOG_E("LoRaTester", "Failed to acquire serial handle after %d attempts", max_retries);
        cleanup_receive_context(app);
        return;
    }

    uint32_t current_baud_rate = get_current_baud_rate(app);
    FURI_LOG_D("LoRaTester", "Initializing UART with baud rate: %lu", current_baud_rate);
    furi_hal_serial_init(serial_handle, current_baud_rate);
    furi_hal_serial_async_rx_start(serial_handle, uart_on_irq_cb, app, true);

    if(app->worker_thread) {
        FURI_LOG_D("LoRaTester", "Freeing existing worker thread");
        furi_thread_free(app->worker_thread);
    }
    FURI_LOG_D("LoRaTester", "Creating new worker thread");
    app->worker_thread = furi_thread_alloc_ex("LoRaReceiverWorker", 1024, uart_worker, app);
    furi_thread_start(app->worker_thread);

    FURI_LOG_D("LoRaTester", "Switching to text box view");
    view_dispatcher_switch_to_view(app->view_dispatcher, LoraTesterAppViewTextBox);
    FURI_LOG_I(
        "LoRaTester", "Receive scene setup complete with baud rate: %lu", current_baud_rate);
}

bool lora_tester_scene_receive_on_event(void* context, SceneManagerEvent event) {
    LoraTesterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == LoraTesterCustomEventRefreshView) {
            if(app->receive_context && app->receive_context->mutex) {
                furi_mutex_acquire(app->receive_context->mutex, FuriWaitForever);
                text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
                furi_mutex_release(app->receive_context->mutex);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        FURI_LOG_D("LoRaTester", "Back button pressed in receive scene");
        cleanup_uart();
        if(app->worker_thread) {
            FURI_LOG_D("LoRaTester", "Stopping worker thread");
            furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerEventStop);
            furi_thread_join(app->worker_thread);
            furi_thread_free(app->worker_thread);
            app->worker_thread = NULL;
        }
        cleanup_receive_context(app);
        reset_text_box(app);
        consumed = false;
    }

    return consumed;
}

void lora_tester_scene_receive_on_exit(void* context) {
    FURI_LOG_I("LoRaTester", "Exiting receive scene");
    LoraTesterApp* app = context;

    cleanup_uart();
    if(app->worker_thread) {
        FURI_LOG_D("LoRaTester", "Stopping worker thread");
        furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerEventStop);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    cleanup_receive_context(app);
    reset_text_box(app);

    FURI_LOG_I("LoRaTester", "Receive scene cleanup complete");
}

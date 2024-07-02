#pragma once

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/text_box.h>
#include <gui/modules/file_browser.h>
#include "scenes/lora_tester_scene.h"
#include "uart_text_input.h"
#include <dialogs/dialogs.h>
#include <gui/modules/dialog_ex.h>
#include <storage/storage.h>
#include <furi_hal.h>
#include <gui/modules/popup.h>
#include <gui/modules/byte_input.h>

#define TEXT_INPUT_STORE_SIZE 128
#define LORA_TESTER_TEXT_BOX_STORE_SIZE 4096

#ifndef COUNT_OF
#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
#endif

extern const char* lora_mode_names[];
typedef struct LoraTesterUart LoraTesterUart;
typedef void (*LoraTesterUartRxCallback)(uint8_t* buf, size_t len, void* context);

extern const uint32_t baud_rates[];
extern const uint8_t baud_rate_count;

typedef enum {
    LoRaMode_Normal,
    LoRaMode_WOR_Transmit,
    LoRaMode_WOR_Receive,
    LoRaMode_Config
} LoRaMode;

typedef enum {
    ConfigureItemAddress,
    ConfigureItemUARTRate,
    ConfigureItemAirDataRate,
    ConfigureItemSubPacketSize,
    ConfigureItemRSSIAmbient,
    ConfigureItemTxPower,
    ConfigureItemChannel,
    ConfigureItemRSSIByte,
    ConfigureItemTransmissionMethod,
    ConfigureItemWORCycle,
    ConfigureItemEncryptionKey,
    ConfigureItemSave,
    ConfigureItemCount
} ConfigureItem;

typedef struct {
    VariableItem* items[ConfigureItemCount];
} ConfigureItemsList;

typedef struct {
    FuriStreamBuffer* rx_stream;
    FuriMutex* mutex;
} ReceiveContext;

struct LoraTesterUart {
    FuriHalSerialHandle* handle;
    FuriMutex* mutex;
};

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    VariableItemList* var_item_list;
    Widget* widget;
    TextBox* text_box;
    FuriString* text_box_store;
    LoRaMode current_mode;
    UART_TextInput* text_input;
    char text_input_store[TEXT_INPUT_STORE_SIZE];
    uint8_t rx_buffer[256];
    ConfigureItemsList* config_items;
    FileBrowser* file_browser;
    FuriString* file_path;
    DialogsApp* dialogs;
    Storage* storage;
    DialogEx* dialog_ex;
    Popup* popup;
    LoraTesterUart* uart;
    ReceiveContext* receive_context;
    FuriThread* worker_thread;
    uint32_t baud_rate;
    uint16_t address;
    ByteInput* byte_input;
    uint16_t encryption_key;
} LoraTesterApp;

typedef enum {
    LoraTesterAppViewVarItemList,
    LoraTesterAppViewWidget,
    LoraTesterAppViewTextBox,
    LoraTesterAppViewTextInput,
    LoraTesterAppViewFileBrowser,
    LoraTesterAppViewDialogEx,
    LoraTesterAppViewPopup,
    LoraTesterCustomEventRefreshView,
    LoraTesterAppViewByteInput,
} LoraTesterAppView;

typedef enum {
    LoraTesterCustomEventTextInputDone,
    LoraTesterCustomEventFileSelected,
    LoraTesterCustomEventOk,
    LoraTesterCustomEventByteInputDone,
} LoraTesterCustomEvent;

void lora_tester_set_mode(LoraTesterApp* app, LoRaMode mode);

typedef enum {
    WorkerEventStop = (1 << 0),
    WorkerEventRx = (1 << 1),
} WorkerEventFlags;

#define WORKER_ALL_EVENTS (WorkerEventStop | WorkerEventRx)

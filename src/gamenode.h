#ifndef GAMENODE_H
#define GAMENODE_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _gamenode gamenode;
typedef enum gamenodeEventType
{
  GAMENODE_CONNECTED,
  GAMENODE_DISCONNECTED,
  GAMENODE_ERROR,
  GAMENODE_RESPONSE,
  GAMENODE_METHOD_CALL
} gamenodeEventType;

typedef struct gamenodeEvent {
  gamenodeEventType type;
  union
  {
    struct
    {

    } connected;
    struct
    {

    } disconnected;
    struct
    {

    } error;
    struct
    {
      long id;
      struct JSON_Value* value;
    } response;
    struct
    {
      long id;
      char const* methodName;
      struct JSON_Value* params;
    } methodCall;
  };
} gamenodeEvent;

typedef void (*gamenodeCallbackFunc)(gamenode*, gamenodeEvent const*);

gamenode* gamenodeNew(gamenodeCallbackFunc callback);
void gamenodeFree(gamenode* gn);
char gamenodeConnect(gamenode* gn,
                     const char *address,
                     int port,
                     const char *path,
                     const char *host,
                     const char *origin);
void gamenodeDisconnect(gamenode* gn);
char gamenodeHandle(gamenode* gn);

void gamenodeSetUserData(gamenode* gn, void* data);
void* gamenodeUserData(gamenode* gn);

long int gamenodeMethodCall(gamenode* gn, char const* methodName, struct JSON_Value* params);
void gamenodeResponse(gamenode* gn, long int msgId, struct JSON_Value* value);

#ifdef __cplusplus
}
#endif

#endif

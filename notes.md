# ERRORS

- bind requests may be incorrect
- maybe need to handle unhandled messages better (not just print message_bytes)
- maybe need to do something for the messages that don't match the interfaces,
  but are still globals
- maybe parsing error (global_message = 41 error?)
- maybe need to keep reading after one of the errors (don't break?)
- check padding

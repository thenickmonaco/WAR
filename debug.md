- bind request format
    - header (all messages have this)
        - object_id (u32): id of resource ('object') we want to call method on
        - opcode (u16): the opcode of the method we want to call
        - size (u16): the size of the message
    - body
        - name (u32, also "global_name")
        - interface name length (u32, length includes null terminator and
          padding, see below)
        - interface name (dynamic, padding to a 32-bit boundary, has null
          value = 0 at end, could be switched with version maybe)
        - version (u32, could be last maybe)
        - new_id (u32, could be moved up maybe): "next_id += 1"

- Binding interface 'wl_shm' (version 2), global_name=1, new_id=3, msg_bytes=
  [2, 0, 0, 0, 0, 0, 28, 0, 1, 0, 0, 0, 7, 0, 0, 0, 119, 108, 95, 115, 104, 109,
  0, 0, 2, 0, 0, 0, 3, 0, 0, 0], msg_len=32
- something written
    - header
        - object_id (u32): 2
        - opcode (u16): 0
        - size (u16): 28
    - body
        - name (u32, also "global_name"):
        - interface name length (u32, length includes null terminator and
          padding):
        - interface name (dynamic, padding to a 32-bit boundary, has null
          value = 0 at end):
        - version (u32, could be last maybe):
        - new_id (u32, could be moved up maybe):

- Binding interface 'zwp_linux_dmabuf_v1' (version 4), global_name=2, new_id=4,
  msg_bytes=[2, 0, 0, 0, 0, 0, 40, 0, 2, 0, 0, 0, 20, 0, 0, 0, 122, 119, 112,
  95, 108, 105, 110, 117, 120, 95, 100, 109, 97, 98, 117, 102, 95, 118, 49, 0,
  4, 0, 0, 0, 4, 0, 0, 0], msg_len=44
  something written -

- Binding interface 'wl_compositor' (version 6), global_name=4, new_id=5,
  msg_bytes=[2, 0, 0, 0, 0, 0, 36, 0, 4, 0, 0, 0, 14, 0, 0, 0, 119, 108, 95, 99,
  111, 109, 112, 111, 115, 105, 116, 111, 114, 0, 0, 0, 6, 0, 0, 0, 5, 0, 0, 0],
  msg_len=40
  something written -

- Binding interface 'xdg_wm_base' (version 5), global_name=12, new_id=6,
  msg_bytes=[2, 0, 0, 0, 0, 0, 32, 0, 12, 0, 0, 0, 12, 0, 0, 0, 120, 100, 103,
  95, 119, 109, 95, 98, 97, 115, 101, 0, 5, 0, 0, 0, 6, 0, 0, 0], msg_len=36
  something written -

- Binding interface 'wl_seat' (version 9), global_name=52, new_id=7, msg_bytes=
  [2, 0, 0, 0, 0, 0, 28, 0, 52, 0, 0, 0, 8, 0, 0, 0, 119, 108, 95, 115, 101, 97,
  116, 0, 9, 0, 0, 0, 7, 0, 0, 0], msg_len=32
  something written -

- Unhandled non-global message: object_id = 1, opcode = 0, bytes =
  [1, 0, 0, 0, 0, 0, 64, 0, 1, 0, 0, 0, 1, 0, 0, 0, 41, 0, 0, 0, 105, 110, 118,
  97, 108, 105, 100, 32, 97, 114, 103, 117, 109, 101, 110, 116, 115, 32, 102,
  111, 114, 32, 119, 108, 95, 114, 101, 103, 105, 115, 116, 114, 121, 35, 50,
  46, 98, 105, 110, 100, 0, 0, 0, 0] -

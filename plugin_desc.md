# GCC phase reorder plugin 2.0

This version of plugin is an backward-compatible improvement of previous version plugin that was used for initial tests and genetic algorithm implementation.
Plugin interface was improved to accommodate for per-function phase reordering and better suit reinforcement learning needs.

## dyn\_replace mode

'dyn\_replace' mode utilises unix sockets to send function names and embeddings 
and receive pass lists that will be used just for this function.

This mode is chosen by specifying `dyn_replace` argument for plugin.

'learning' and 'inference' (inference is the default) are two sub-modes of 'dyn\_replace' that impact 
the order of send and receive operations.
In learning mode first the name of the function is send, then a list for it is received, and the new embedding is sent.
In inference mode embedding is sent, and list for the current function is received (remote side is meant to calculate it based on embedding).

## Introduced constraints

GCC Plugin API does not provide ways to delete unneeded passes from the pass tree, and the internal structure does not provide any ways to cleanly delete pass objects
without leaving dangling pointers to them. However with per-function phase reorder freeing memory used by pass object becomes a necessity,
as otherwise all the unneeded, but not deleted passes would take too much memory.

Studying GCC pass manager sources showed, that in case of naively calling `delete` on unneeded pass objects will leave dangling pointers
to them and these pointers cannot be removed without patching GCC code.
However they are only dereferenced when `-fdump-passes` flag is used.
As we expect this plugin to be used only for final 'release' compilation, inability to use this flag should not impact existing workflows.

## Internal plugin operation

### Communication

Plugin utilizes unix datagram sockets for communication during compilation.
Plugin creates socket 'gcc\_plugin.soc' socket in its working directory and binds to socket provided using `remote_socket` argument.
The order and contents of send-receive operations depend on sub-mode: learning or inference.
In learning mode the plugin sends current function name to the socket and blocks until pass list is received. Then plugin applies the pass list and sends the new embedding.
In inference mode no function name is sent, instead plugin sends out initial function embedding (before the list), and blocks until pass list is received.
No other messages are sent for the same function.

Pass lists are received by pass `list_recv_pass` (inserted with name 'dynamic\_list\_insert').
Pass list format is the same as it was for pass list files.
Also two special chars are recognised: '\0' and '?'.
'\0' causes the initial (GCC default) pass list to be used (useful for getting baseline function embedding and stats).
'?' causes the second pass list (which we are operating on) to be emptied and not filled with any passes (useful for getting initial state for reinforcement learning).

Function embeddings are sent by pass `embedding_send_pass` (inserted with name 'embed\_send').
Embedding has format:
 - Autophase part (47 integers)
 - Control-flow graph length (1 integer)
 - Control-flow graph
 - Data-flow graph
 
Graph are transported as adjacency matrices in form of coordinate lists.

Function name is sent by pass `func_name_send_pass` (inserted with name 'func\_name\_send').

### Passes

#### `list_recv_pass`

This pass is responsible for receiving the pass list and altering the pass tree.
First, it blocks waiting for pass list message.
Max message size is 4096 bytes, anything longer will be truncated.
After the message is received, pass iterates through the pass tree to locate marker pass '\*plugin\_dummy\_pass\_end\_list'.
This marker pass is placed by plugin after the end of the pass list it operates on.

If the pass tree is in its initial state, it is not deleted, but pointer at list head is saved and the tree is joined bypassing this pass list. If the pass list has been altered, it is separated from the tree and all the passes in it are deleted to preserve memory.

Then, based on first byte of pass list, the list is operated on, input buffer is cleared and the pass returns.

#### `func_name_send_pass`

This pass utilizes GCC macro `IDENTIFIER_POINTER` to get current function name.
This macro essentially expands to language frontend hook, so c++ symbol names are returned in mangled state.

Some GCC passes add postfixes to functions.
These postfixes are always separated with a '.', and pass drops them out when function name is printed to stdout.
To the socket function name is sent without as is.


#### `embedding_send_pass`

This pass calculates function embedding and sends it to the socket.
Function calls that gather function information and calculate the embeddings should be documented in some other file.
All embeddings are collected in a buffer, 47-nth integer is set to control-flow graph length to be able to split the buffer correctly on the receiving side.

### `plugin_init`

If 'dyn\_replace' is specified in plugin arguments, plugin initializes for per-function pass reordering.

First, unix datagram socket is created and bound to './gcc\_plugin.soc' in working directory. Then plugin looks for remote socket name specified with 'remote\_socket' argument and connects to it, to avoid receiving messages from other sockets.

After socket setup, two passes are inserted: `list_recv_pass` at the start of the list and a marker pass '\*plugin\_dummy\_pass\_end\_list' at the end. 
If argument 'dyn\_replace' has value 'learning' `embedding_send_pass` is inserted after the marker pass, and `func_name_send_pass` is inserted before `list_recv_pass`.
Otherwise only `embedding_send_pass` is inserted before `list_recv_pass`.

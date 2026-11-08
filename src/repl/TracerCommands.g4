grammar TracerCommands;

line : '%' command EOF;
command : server | attach | processes | sessions | use | status | detach
        | modules | threads | symbols | probe | interceptor | events | collect | record
        | cpp | shell | complete | type | help | quit | bulkTrace | memory | probeEntry | probeReload
        | targetLaunch | targetCall | localPerf | instructions | watchpoint
        | targetLibrary | childList;
server : 'server' ('start' serverOption* | 'connect' value | 'status' | 'stop');
serverOption : ('--executable' | '--agent') value | contextEventBytes;
attach : 'attach' (UINT ('--start-time' UINT)?)?;
processes : 'processes' ('--filter' value)?;
sessions : 'sessions';
use : 'use' value;
status : 'status';
detach : 'detach' value?;
modules : 'modules' ('--details' | '--elf' value | '--watch' (('--timeout' | '--count') UINT)*)?;
threads : 'threads';
symbols : 'symbols' symbolOption*;
symbolOption : ('--filter' | '--module' | '--page' | '--address' | '--mapping') value
             | '--limit' UINT | '--addresses';
probe : 'probe' ('prepare' value prepareOption+
      | 'resolve' value (resolveOption | cwdOption)*
      | 'inject' value '--callback' value (deployOption | cwdOption)*
      | 'validate' value '--callback' value (validateOption | cwdOption)*
      | 'remove' UINT cwdOption?) | 'probes' cwdOption?;
prepareOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--output') value
              | METADATA_ONLY | extraCompileOption | cwdOption;
resolveOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--compile-option'
                | '--prepared' | '--prepared-mode') value
              | extraCompileOption;
validateOption : resolveOption | '--allow-arg-overrides' | '--allow-result-override' | '--allow-replacement'
               | '--reclaimable' | '--allow-call-original' | '--trace-exceptions';
deployOption : validateOption | '--stack-depth' (UINT | WORD)
             | '--print-arguments' | '--print-result' | stackFilter;
interceptor : 'interceptor' ('load' value '--as' value | 'edit' value | 'show' value
      | 'validate' value '--target' value validateOption*
            | 'upload' value '--target' value deployOption*);
events : 'events' (streamOption*
       | 'list' value ('--limit' UINT)?
       | 'show' value '--index' UINT
       | STACK value '--index' UINT
       | 'calls' value | 'stats' value | 'clear' value
       | 'open' value '--as' value
       | 'export' value '--format' 'json' '--output' value);
collect : 'collect' streamOption*;
record : 'record' value (streamOption | SQLITE)*;
streamOption : ('--count' | '--timeout' | '--after' | '--probe') UINT
             | '--phase' ('PRE' | 'POST' | 'EXCEPTION')
             | ('--as' | '--name' | '--session') value | '--all';
cpp : 'cpp' ('load' value | 'reset'
            | ('import' | 'generate') value (('--type' | '--output') value)*
            | 'options' ('add' (value | SHELL)+ | 'clear'
                        | 'compdb' value '--file' value)?);
shell : 'shell' value;
complete : 'complete' value;
type : 'type' value;
help : 'help' ('%'? value+)?;
quit : 'quit' | 'exit';
bulkTrace : 'trace' ('preview' bulkTraceOption+
          | 'apply' '--manifest' value ('--callback' value)?
          | 'remove' value);
bulkTraceOption : ('--module' | '--include' | '--exclude' | '--exact' | '--manifest' | '--save') value;
value : STRING | WORD | UINT | keyword;
keyword : 'server' | 'start' | 'connect' | 'status' | 'stop' | 'attach'
        | 'processes' | 'sessions' | 'use' | 'detach' | 'modules' | 'threads' | 'symbols'
        | 'probe' | 'prepare' | 'resolve' | 'inject' | 'validate' | 'remove' | 'probes'
        | 'interceptor' | 'load' | 'edit' | 'show' | 'upload' | 'events'
        | 'list' | 'calls' | 'stats' | 'clear' | 'open' | 'export' | 'json'
        | 'collect' | 'record' | 'PRE' | 'POST' | 'EXCEPTION' | 'cpp' | 'reset' | 'import'
        | 'generate' | 'options' | 'add' | 'compdb' | 'shell'
        | 'complete' | 'type' | 'help' | 'quit' | 'exit' | 'trace' | 'preview' | 'apply'
        | 'memory' | 'maps' | 'read' | 'scan' | 'patch' | 'restore' | 'import-hook'
        | 'monitor-start' | 'monitor-stop'
        | 'entry' | 'reload' | 'launch' | 'resume' | 'terminate'
        | 'call' | 'call-result' | 'syscalls' | 'sample' | 'cpu' | 'instructions' | 'watchpoint'
        | 'library' | 'allocate' | 'free' | 'write' | 'replay' | 'children' | 'faults' | 'allocations' | STACK;
memory : 'memory' ('maps' | 'read' value '--mapping' value
                 ('--count' UINT | '--type' value)
                 | 'scan' value '--mapping' value '--count' UINT
                   '--pattern' value ('--limit' UINT)?
                 | 'patch' '--module' value '--elf-address' value '--mapping' value '--expect' value '--replace' value
                 | 'write' value '--mapping' value '--expect' value '--replace' value
                 | ('restore' | 'allocate' | 'free') UINT
                 | 'monitor-start' '--tid' UINT '--address' value '--size' UINT '--mapping' value ('--timeout' UINT)?
                 | 'monitor-stop'
                 | 'import-hook' '--module' value '--symbol' value '--original' value
                   '--address' value '--mapping' value '--abi' value);
probeEntry : 'probe' 'entry' (value '--mapping' value ('--module' value)?
           | '--module' value '--elf-address' value '--abi' value '--mapping' value) cwdOption?;
probeReload : 'probe' 'reload' UINT '--callback' value
              (cwdOption | '--watch' (('--timeout' | '--count') UINT)*)*;
targetLaunch : 'launch' value launchOption* | ('resume' | 'terminate') UINT?;
launchOption : ('--arg' | '--cwd' | '--env' | '--stdin' | '--stdout' | '--stderr' | '--preload') value
             | '--timeout' UINT | '--supervise' | '--gate-children';
childList : 'children';
targetCall : 'call' value ('--arg' value | '--timeout' UINT)* | 'call-result' ('--timeout' UINT)?;
localPerf : ('syscalls' | 'faults' | 'sample' ('cpu' | 'allocations')) (('--count' | '--timeout') UINT)*;
targetLibrary : 'library' 'load' value;
instructions : 'instructions' ('start' instructionOption* | 'stop' ('--save' value)? | 'replay' value);
instructionOption : ('--tid' | '--size' | '--count' | '--timeout') UINT
                  | '--memory-size' UINT | '--registers' | '--discover'
                  | ('--address' | '--mapping' | '--callback-library' | '--memory-address') value;
watchpoint : 'watchpoint' (('--tid' | '--size' | '--count' | '--timeout') UINT
                         | ('--address' | '--mapping' | '--access') value)*;
stackFilter : '--stack-application-only' value;
contextEventBytes : ('--context-event-bytes' | '--context-transport-memory-bytes') UINT;
extraCompileOption : '--extra-arg' value | '--extra-args' (value | SHELL)+;
cwdOption : CWD value;
UINT : [0-9]+;
STRING : '"' ('\\' ["\\nrt] | ~["\\\r\n])* '"'
       | '\'' ('\\' ['\\nrt] | ~['\\\r\n])* '\'';
SHELL : '`' ~[`\r\n]+ '`';
METADATA_ONLY : '--metadata-only';
SQLITE : '--sqlite';
STACK : 'stack';
CWD : '--cwd';
WORD : ~[ \t\r\n"'\\%`]+;
WS : [ \t\r\n]+ -> skip;

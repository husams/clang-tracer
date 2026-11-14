grammar TracerCommands;

line : '%' command EOF;
command : server | attach | processes | sessions | use | status | detach
        | modules | moduleIndex | threads | symbols | probe | interceptor | events | collect | record
        | cpp | shell | complete | type | help | quit | bulkTrace | memory | probeEntry | probeReload
        | objectWatch | liveUpdate;
server : 'server' ('start' serverOption* | 'connect' value | 'status' | 'stop');
serverOption : ('--executable' | '--agent') value | contextEventBytes;
attach : 'attach' (UINT ('--start-time' UINT)?)?;
processes : 'processes' ('--filter' value)?;
sessions : 'sessions';
use : 'use' value;
status : 'status';
detach : 'detach' value?;
modules : 'modules' '--details'?;
moduleIndex : 'module' ('index' moduleBuildOption+
            | 'use' value
            | ('types' | 'symbols') moduleQueryOption*
            | 'show' moduleShowOption+
            | 'emit' moduleEmitOption+);
moduleBuildOption : ('--file' | '--module' | '--mapping' | '--output' | '--debug-file') value
                  | ('--max-entries' | '--max-depth') UINT;
moduleQueryOption : ('--index' | '--filter' | '--page') value
                  | '--limit' UINT | '--format' 'json' | '--exported';
moduleShowOption : ('--index' | '--type' | '--symbol' | '--id') value;
moduleEmitOption : moduleShowOption | '--output' value;
threads : 'threads';
symbols : 'symbols' symbolOption*;
symbolOption : ('--filter' | '--module' | '--page' | '--address' | '--mapping') value
             | '--limit' UINT | '--addresses';
probe : 'probe' ('prepare' value prepareOption+
      | 'resolve' value (resolveOption | cwdOption)*
      | 'inject' value ('--callback' value)? (deployOption | cwdOption)*
      | 'validate' value '--callback' value (resolveOption | cwdOption)*
      | 'remove' UINT cwdOption?) | 'probes' cwdOption?;
prepareOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--output' | '--module-index' | '--generated' | '--id') value
              | SIGNATURE value | MEMBER | METADATA_ONLY | extraCompileOption | cwdOption;
resolveOption : ('--include' | '--compdb' | '--translation-unit' | '--module' | '--compile-option'
                | '--prepared' | '--prepared-mode') value
              | extraCompileOption;
deployOption : resolveOption | '--stack-depth' (UINT | WORD)
             | '--print-arguments' | '--print-result' | SNAPSHOT_RETURN_POINTEE
             | '--snapshot-receiver-at-return' | stackFilter
             | ('--backend' | '--caller') value;
interceptor : 'interceptor' ('load' value '--as' value | 'edit' value | 'show' value
      | 'validate' value '--target' value resolveOption*
            | 'upload' value '--target' value deployOption*);
events : 'events' (streamOption*
       | 'list' value ('--limit' UINT)?
       | 'show' value '--index' UINT
       | 'diff' value '--pre' UINT '--post' UINT ('--key' value)*
       | STACK value '--index' UINT
       | 'calls' value | 'stats' value | 'clear' value
       | 'open' value '--as' value
       | 'export' value '--format' 'json' '--output' value);
collect : 'collect' streamOption*;
record : 'record' value (streamOption | SQLITE)*;
streamOption : ('--count' | '--timeout' | '--after' | '--probe') UINT
             | '--phase' ('PRE' | 'POST')
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
        | 'module' | 'index' | 'types' | 'emit'
        | 'probe' | 'prepare' | 'resolve' | 'inject' | 'validate' | 'remove' | 'probes'
        | 'interceptor' | 'load' | 'edit' | 'show' | 'upload' | 'events'
        | 'list' | 'calls' | 'stats' | 'clear' | 'open' | 'export' | 'json'
        | 'collect' | 'record' | 'PRE' | 'POST' | 'cpp' | 'reset' | 'import'
        | 'generate' | 'options' | 'add' | 'compdb' | 'shell'
        | 'complete' | 'type' | 'help' | 'quit' | 'exit' | 'trace' | 'preview' | 'apply'
        | 'memory' | 'maps' | 'read' | 'scan' | 'entry' | 'reload' | 'object'
        | 'live' | 'update' | 'revert' | 'mock' | 'update-data' | 'scan-heap' | STACK;
liveUpdate : 'live' (('update' | 'preview') (value | liveOption)+ | 'status' | 'revert' UINT?
                   | 'mock' value liveOption+ | 'update-data' value (liveOption | liveScanSelection)*
                   | 'scan-heap' liveOption+);
liveScanSelection : '--from-scan' value+;
liveOption : ('--build' | '--compdb' | '--file' | '--function' | '--fixture' | '--bind'
              | '--recursion' | '--discover-file' | '--discover-function' | '--module'
              | '--address' | '--type'
              | '--after-file' | '--after-function' | '--for-each' | '--managed') value
           | '--retention-limit' UINT;
objectWatch : 'object' ('prepare' value? objectOption+ | ('start' | 'preview') objectOption+
                        | ('stop' | 'status') UINT?);
objectOption : ('--type' | '--base' | '--owner' | '--field' | '--collection'
                 | '--owner-add' | '--owner-get' | '--owner-remove' | '--owner-snapshot'
                 | '--snapshot-field' | '--code' | '--backend' | '--module' | '--caller'
                 | '--plan' | '--output' | '--executable' | '--mode'
                 | '--include' | '--policy' | '--compdb' | '--translation-unit'
                 | '--on' | '--key' | '--emit-source'
                 | '--vector' | '--element-key' | '--map' | '--map-key' | '--pointee'
                 | '--mutation' | '--synchronization' | '--union-tag' | '--union-case'
                 | '--rendezvous' | '--overflow' | '--shared-file') value
             | ('--stack-depth' | '--max-elements' | '--max-string-bytes'
                 | '--max-depth' | '--max-snapshot-bytes' | '--max-instances'
                 | '--max-events' | '--event-buffer' | '--timeout' | '--tid' | '--async-signal'
                 | '--serialize-wait-us' | '--runtime-limit') UINT
             | '--no-values' | '--first-seen' | '--writer-entry' | '--discover-writers' | '--follow-calls'
             | '--syscall-output' | '--stack-cfi';
memory : 'memory' ('maps' | 'read' value '--mapping' value
                 ('--count' UINT | '--type' value)
                 | 'scan' value '--mapping' value '--count' UINT
                   '--pattern' value ('--limit' UINT)?);
probeEntry : 'probe' 'entry' (value '--mapping' value ('--module' value)?
           | '--module' value '--elf-address' value '--abi' value '--mapping' value) cwdOption?;
probeReload : 'probe' 'reload' UINT '--callback' value cwdOption?;
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
SIGNATURE : '--signature';
MEMBER : '--member';
SNAPSHOT_RETURN_POINTEE : '--snapshot-return-pointee';
WORD : ~[ \t\r\n"'\\%`]+;
WS : [ \t\r\n]+ -> skip;

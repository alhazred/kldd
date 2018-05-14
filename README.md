kldd
====

Print dependencies of illumos kernel modules

#### Usage
<pre>
Usage: kldd [-K] file ...
</pre>
Example output:
<pre>
# /usr/bin/kldd /kernel/fs/amd64/nfs
        fs/specfs =>    /kernel/fs/amd64/specfs
        strmod/rpcmod =>        /kernel/strmod/amd64/rpcmod
        misc/rpcsec =>  /kernel/misc/amd64/rpcsec
        unix (parent) =>        /platform/i86pc/kernel/amd64/unix
        genunix (parent dependency) =>  /kernel/amd64/genunix
</pre>

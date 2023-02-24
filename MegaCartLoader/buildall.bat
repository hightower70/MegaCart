@sjasmplus.exe -Wno-rdlow --raw=megacart_loader.bin --syntax=abf -DMULTICART=0 -DDECOMPRESSOR_ENABLED=0 megacart.a80
@sjasmplus.exe -Wno-rdlow --raw=megacart_decomp_loader.bin --syntax=abf -DMULTICART=0 -DDECOMPRESSOR_ENABLED=1 megacart.a80
@bin2c -o megacart_loader.c megacart_loader.bin 
@bin2c -o megacart_decomp_loader.c megacart_decomp_loader.bin 
@copy megacart_loader.c "../MegaCartImageBuilder/Source Files/megacart_loader.c"
@copy megacart_decomp_loader.c "../MegaCartImageBuilder/Source Files/megacart_decomp_loader.c"

@sjasmplus.exe -Wno-rdlow --raw=multicart_loader.bin --syntax=abf -DMULTICART=1 -DDECOMPRESSOR_ENABLED=0 megacart.a80
@sjasmplus.exe -Wno-rdlow --raw=multicart_decomp_loader.bin --syntax=abf -DMULTICART=1 -DDECOMPRESSOR_ENABLED=1 megacart.a80
@bin2c -o multicart_loader.c multicart_loader.bin 
@bin2c -o multicart_decomp_loader.c multicart_decomp_loader.bin 
@copy multicart_loader.c "../MegaCartImageBuilder/Source Files/multicart_loader.c"
@copy multicart_decomp_loader.c "../MegaCartImageBuilder/Source Files/multicart_decomp_loader.c"
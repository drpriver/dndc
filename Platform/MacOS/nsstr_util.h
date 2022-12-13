#import <Foundation/Foundation.h>
#ifndef DNDC_API
#define DNDC_API static inline
#endif
#import "Dndc/dndc_long_string.h"
#import "Utils/MStringBuilder.h"

#pragma clang assume_nonnull begin

static
NSString*_Nonnull
msb_detach_as_ns_string(MStringBuilder*sb){
    StringView text = msb_detach_sv(sb);
    PushDiagnostic();
    SuppressCastQual();
    NSData* data = [NSData dataWithBytesNoCopy:(void*)text.text length:text.length freeWhenDone:YES];
    PopDiagnostic();
    NSString* str = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    return str;
}

static inline
NSString*
ns_consume_ls(LongString ls){
    PushDiagnostic();
    SuppressCastQual();
    NSData* data = [NSData dataWithBytesNoCopy:(void*)ls.text length:ls.length freeWhenDone:YES];
    PopDiagnostic();
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

static
StringView
ns_borrow_sv(NSString*_Nullable str){
    if(!str) return SV("");
    const char* text = [str UTF8String];
    size_t len = strlen(text);
    return (StringView){.text=text, .length=len};
}

#pragma clang assume_nonnull end

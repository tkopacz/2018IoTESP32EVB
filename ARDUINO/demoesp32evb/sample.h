// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SAMPLE_H
#define SAMPLE_H

#ifdef __cplusplus
extern "C" {
#endif

    void sample_run(void);
    
    //Initilization - iot handle + platform
    void tk_init(void);

    void tkSendMessagePublic(int Analog, int Touch, bool PIR,int dt_epoch_s);
    
    int tkGetDelay();
    int tkGetShape();
    
#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_H */

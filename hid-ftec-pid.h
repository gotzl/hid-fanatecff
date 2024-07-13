0x35, 0x00,                     /*      Physical Minimum (0),               */
0x45, 0x00,                     /*      Physical Maximum (0),               */
0x05, 0x0F,                     /*      Usage Page (PID),                   */
0x09, 0x92,                     /*      Usage (92h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x10,                     /*          Report ID (16),                 */
0x09, 0x9F,                     /*          Usage (9Fh),                    */
0x09, 0xA0,                     /*          Usage (A0h),                    */
0x09, 0xA4,                     /*          Usage (A4h),                    */
0x09, 0xA5,                     /*          Usage (A5h),                    */
0x09, 0xA6,                     /*          Usage (A6h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x25, 0x01,                     /*          Logical Maximum (1),            */
0x75, 0x01,                     /*          Report Size (1),                */
0x95, 0x05,                     /*          Report Count (5),               */
0x81, 0x02,                     /*          Input (Variable),               */
0x95, 0x03,                     /*          Report Count (3),               */
0x81, 0x03,                     /*          Input (Constant, Variable),     */
0x09, 0x94,                     /*          Usage (94h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x25, 0x01,                     /*          Logical Maximum (1),            */
0x75, 0x01,                     /*          Report Size (1),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x81, 0x02,                     /*          Input (Variable),               */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x07,                     /*          Report Size (7),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x81, 0x02,                     /*          Input (Variable),               */
0xC0,                           /*      End Collection,                     */
0x09, 0x21,                     /*      Usage (21h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x11,                     /*          Report ID (17),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x25,                     /*          Usage (25h),                    */
0xA1, 0x02,                     /*          Collection (Logical),           */
0x09, 0x26,                     /*              Usage (26h),                */
0x09, 0x27,                     /*              Usage (27h),                */
0x09, 0x28,                     /*              Usage (28h),                */
0x09, 0x30,                     /*              Usage (30h),                */
0x09, 0x31,                     /*              Usage (31h),                */
0x09, 0x32,                     /*              Usage (32h),                */
0x09, 0x33,                     /*              Usage (33h),                */
0x09, 0x34,                     /*              Usage (34h),                */
0x09, 0x40,                     /*              Usage (40h),                */
0x09, 0x41,                     /*              Usage (41h),                */
0x09, 0x42,                     /*              Usage (42h),                */
0x09, 0x43,                     /*              Usage (43h),                */
0x15, 0x01,                     /*              Logical Minimum (1),        */
0x25, 0x12,                     /*              Logical Maximum (12),       */
0x75, 0x08,                     /*              Report Size (8),            */
0x95, 0x01,                     /*              Report Count (1),           */
0x91, 0x00,                     /*              Output,                     */
0xC0,                           /*          End Collection,                 */
0x09, 0x50,                     /*          Usage (50h),                    */
0x09, 0xA7,                     /*          Usage (A7h),                    */
0x09, 0x54,                     /*          Usage (54h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x66, 0x03, 0x10,               /*          Unit (Seconds),                 */
0x55, 0xFD,                     /*          Unit Exponent (-3),             */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x03,                     /*          Report Count (3),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x55, 0x00,                     /*          Unit Exponent (0),              */
0x66, 0x00, 0x00,               /*          Unit,                           */
0x09, 0x52,                     /*          Usage (52h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0x64, 0x00,               /*          Logical Maximum (100),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x53,                     /*          Usage (53h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x00,               /*          Logical Maximum (255),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x55,                     /*          Usage (55h),                    */
0xA1, 0x02,                     /*          Collection (Logical),           */
0x05, 0x01,                     /*              Usage Page (Desktop),       */
0x09, 0x30,                     /*              Usage (X),                  */
0x09, 0x31,                     /*              Usage (Y),                  */
// 0x09, 0x32,                     /*              Usage (Z),                  */
0x15, 0x00,                     /*              Logical Minimum (0),        */
0x25, 0x01,                     /*              Logical Maximum (1),        */
0x75, 0x01,                     /*              Report Size (1),            */
0x95, 0x02,                     /*              Report Count (2),           */
0x91, 0x02,                     /*              Output (Variable),          */
0x75, 0x02,                     /*              Report Size (2),            */
0x95, 0x01,                     /*              Report Count (1),           */
0x91, 0x03,                     /*              Output (Constant, Viriable),*/
0xC0,                           /*          End Collection,                 */
0x05, 0x0F,                     /*          Usage Page (PID),               */
0x09, 0x56,                     /*          Usage (56h),                    */
0x75, 0x01,                     /*          Report Size (1),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x75, 0x03,                     /*          Report Size (3),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x03,                     /*          Output (Constant, Variable),    */
0x09, 0x57,                     /*          Usage (57h),                    */
0xA1, 0x02,                     /*          Collection (Logical),           */
0x0B, 0x01, 0x00, 0x0A, 0x00,   /*              Usage (000A0001h),          */
0x0B, 0x02, 0x00, 0x0A, 0x00,   /*              Usage (000A0002h),          */
0x66, 0x14, 0x00,               /*              Unit (Degrees),             */
0x55, 0xFE,                     /*              Unit Exponent (-2),         */
0x15, 0x00,                     /*              Logical Minimum (0),        */
0x27, 0x3C, 0x8C, 0x00, 0x00,   /*              Logical Maximum (35900),    */
0x75, 0x10,                     /*              Report Size (16),           */
0x95, 0x02,                     /*              Report Count (2),           */
0x91, 0x02,                     /*              Output (Variable),          */
0x55, 0x00,                     /*              Unit Exponent (0),          */
0x66, 0x00, 0x00,               /*              Unit,                       */
0xC0,                           /*          End Collection,                 */
0xC0,                           /*      End Collection,                     */
0x05, 0x0F,                     /*      Usage Page (PID),                   */
0x09, 0x5A,                     /*      Usage (5Ah),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x12,                     /*          Report ID (18),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x5B,                     /*          Usage (5Bh),                    */
0x09, 0x5D,                     /*          Usage (5Dh),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x46, 0x10, 0x27,               /*          Physical Maximum (10000),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x02,                     /*          Report Count (2),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x5C,                     /*          Usage (5Ch),                    */
0x09, 0x5E,                     /*          Usage (5Eh),                    */
0x66, 0x03, 0x10,               /*          Unit (Seconds),                 */
0x55, 0xFD,                     /*          Unit Exponent (-3),             */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x02,                     /*          Report Count (2),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0x66, 0x00, 0x00,               /*          Unit,                           */
0x55, 0x00,                     /*          Unit Exponent (0),              */
0xC0,                           /*      End Collection,                     */
0x09, 0x5F,                     /*      Usage (5Fh),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x13,                     /*          Report ID (19),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x23,                     /*          Usage (23h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x25, 0x01,                     /*          Logical Maximum (1),            */
0x75, 0x04,                     /*          Report Size (4),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x75, 0x04,                     /*          Report Size (4),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x60,                     /*          Usage (60h),                    */
0x09, 0x61,                     /*          Usage (61h),                    */
0x09, 0x62,                     /*          Usage (62h),                    */
0x16, 0x00, 0x80,               /*          Logical Minimum (-32767),       */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x36, 0xF0, 0xD8,               /*          Physical Minimum (-10000),      */
0x46, 0x10, 0x27,               /*          Physical Maximum (10000),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x03,                     /*          Report Count (3),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x63,                     /*          Usage (63h),                    */
0x09, 0x64,                     /*          Usage (64h),                    */
0x09, 0x65,                     /*          Usage (65h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x27, 0xFF, 0xFF, 0x00, 0x00,   /*          Logical Maximum (65535),        */
0x35, 0x00,                     /*          Physical Minimum (0),           */
0x46, 0x10, 0x27,               /*          Physical Maximum (10000),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x03,                     /*          Report Count (3),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0xC0,                           /*      End Collection,                     */
0x09, 0x6E,                     /*      Usage (6Eh),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x1D,                     /*          Report ID (29),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x70,                     /*          Usage (70h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x35, 0x00,                     /*          Physical Minimum (0),           */
0x46, 0x10, 0x27,               /*          Physical Maximum (10000),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x6F,                     /*          Usage (6Fh),                    */
0x16, 0x00, 0x80,               /*          Logical Minimum (-32767),       */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x36, 0xF0, 0xD8,               /*          Physical Minimum (-10000),      */
0x46, 0x10, 0x27,               /*          Physical Maximum (10000),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x35, 0x00,                     /*          Physical Minimum (0),           */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0x09, 0x71,                     /*          Usage (71h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x27, 0x3C, 0x8C, 0x00, 0x00,   /*          Logical Maximum (35900),        */
0x66, 0x14, 0x00,               /*          Unit (Degrees),                 */
0x55, 0xFE,                     /*          Unit Exponent (-2),             */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x72,                     /*          Usage (72h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x66, 0x03, 0x10,               /*          Unit (Seconds),                 */
0x55, 0xFD,                     /*          Unit Exponent (-3),             */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x65, 0x00,                     /*          Unit (None),                    */
0x55, 0x00,                     /*          Unit Exponent (0),              */
0xC0,                           /*      End Collection,                     */
0x09, 0x73,                     /*      Usage (73h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x15,                     /*          Report ID (21),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x35, 0x01,                     /*          Physical Minimum (1),           */
0x45, 0x28,                     /*          Physical Maximum (40),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x70,                     /*          Usage (70h),                    */
0x16, 0x00, 0x80,               /*          Logical Minimum (-32767),       */
0x26, 0xFF, 0x7F,               /*          Logical Maximum (32767),        */
0x36, 0xF0, 0xD8,               /*          Physical Minimum (-10000),      */
0x46, 0x10, 0x27,               /*          Physical Maximum (10000),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x35, 0x00,                     /*          Physical Minimum (0),           */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0xC0,                           /*      End Collection,                     */
0x05, 0x0F,                     /*      Usage Page (PID),                   */
0x09, 0x77,                     /*      Usage (77h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x1A,                     /*          Report ID (26),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x35, 0x01,                     /*          Physical Minimum (1),           */
0x45, 0x28,                     /*          Physical Maximum (40),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x35, 0x00,                     /*          Physical Minimum (0),           */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0x09, 0x78,                     /*          Usage (78h),                    */
0xA1, 0x02,                     /*          Collection (Logical),           */
0x09, 0x79,                     /*              Usage (79h),                */
0x09, 0x7A,                     /*              Usage (7Ah),                */
0x09, 0x7B,                     /*              Usage (7Bh),                */
0x15, 0x01,                     /*              Logical Minimum (1),        */
0x25, 0x03,                     /*              Logical Maximum (3),        */
0x75, 0x08,                     /*              Report Size (8),            */
0x95, 0x01,                     /*              Report Count (1),           */
0x91, 0x00,                     /*              Output,                     */
0xC0,                           /*          End Collection,                 */
0x09, 0x7C,                     /*          Usage (7Ch),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x00,               /*          Logical Maximum (255),          */
0x46, 0xFF, 0x00,               /*          Physical Maximum (255),         */
0x91, 0x02,                     /*          Output (Variable),              */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0xC0,                           /*      End Collection,                     */
0x09, 0x90,                     /*      Usage (90h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x1B,                     /*          Report ID (27),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0xC0,                           /*      End Collection,                     */
0x09, 0x96,                     /*      Usage (96h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x1C,                     /*          Report ID (28),                 */
0x09, 0x97,                     /*          Usage (97h),                    */
0x09, 0x98,                     /*          Usage (98h),                    */
0x09, 0x99,                     /*          Usage (99h),                    */
0x09, 0x9A,                     /*          Usage (9Ah),                    */
0x09, 0x9B,                     /*          Usage (9Bh),                    */
0x09, 0x9C,                     /*          Usage (9Ch),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x06,                     /*          Logical Maximum (6),            */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x00,                     /*          Output,                         */
0xC0,                           /*      End Collection,                     */
0x09, 0xAB,                     /*      Usage (ABh),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x14,                     /*          Report ID (20),                 */
0x09, 0x25,                     /*          Usage (25h),                    */
0xA1, 0x02,                     /*          Collection (Logical),           */
0x09, 0x26,                     /*              Usage (26h),                */
0x09, 0x27,                     /*              Usage (27h),                */
0x09, 0x28,                     /*              Usage (28h),                */
0x09, 0x30,                     /*              Usage (30h),                */
0x09, 0x31,                     /*              Usage (31h),                */
0x09, 0x32,                     /*              Usage (32h),                */
0x09, 0x33,                     /*              Usage (33h),                */
0x09, 0x34,                     /*              Usage (34h),                */
0x09, 0x40,                     /*              Usage (40h),                */
0x09, 0x41,                     /*              Usage (41h),                */
0x09, 0x42,                     /*              Usage (42h),                */
0x09, 0x43,                     /*              Usage (43h),                */
0x15, 0x01,                     /*              Logical Minimum (1),        */
0x25, 0x12,                     /*              Logical Maximum (12),       */
0x75, 0x08,                     /*              Report Size (8),            */
0x95, 0x01,                     /*              Report Count (1),           */
0xB1, 0x00,                     /*              Feature,                    */
0xC0,                           /*          End Collection,                 */
0x05, 0x01,                     /*          Usage Page (Desktop),           */
0x09, 0x3B,                     /*          Usage (Byte Count),             */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x01,               /*          Logical Maximum (511),          */
0x46, 0xFF, 0x01,               /*          Physical Maximum (511),         */
0x75, 0x0A,                     /*          Report Size (10),               */
0x95, 0x01,                     /*          Report Count (1),               */
0xB1, 0x02,                     /*          Feature (Variable),             */
0x75, 0x06,                     /*          Report Size (6),                */
0xB1, 0x01,                     /*          Feature (Constant),             */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0xC0,                           /*      End Collection,                     */
0x05, 0x0F,                     /*      Usage Page (PID),                   */
0x09, 0x89,                     /*      Usage (89h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x16,                     /*          Report ID (22),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x35, 0x01,                     /*          Physical Minimum (1),           */
0x45, 0x28,                     /*          Physical Maximum (40),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0xB1, 0x02,                     /*          Feature (Variable),             */
0x09, 0x8B,                     /*          Usage (8Bh),                    */
0xA1, 0x02,                     /*          Collection (Logical),           */
0x09, 0x8C,                     /*              Usage (8Ch),                */
0x09, 0x8D,                     /*              Usage (8Dh),                */
0x09, 0x8E,                     /*              Usage (8Eh),                */
0x25, 0x03,                     /*              Logical Maximum (3),        */
0x15, 0x01,                     /*              Logical Minimum (1),        */
0x35, 0x01,                     /*              Physical Minimum (1),       */
0x45, 0x03,                     /*              Physical Maximum (3),       */
0x75, 0x08,                     /*              Report Size (8),            */
0x95, 0x01,                     /*              Report Count (1),           */
0xB1, 0x00,                     /*              Feature,                    */
0xC0,                           /*          End Collection,                 */
0x09, 0xAC,                     /*          Usage (ACh),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x27, 0xFF, 0xFF, 0x00, 0x00,   /*          Logical Maximum (65535),        */
0x35, 0x00,                     /*          Physical Minimum (0),           */
0x47, 0xFF, 0xFF, 0x00, 0x00,   /*          Physical Maximum (65535),       */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0xB1, 0x00,                     /*          Feature,                        */
0x45, 0x00,                     /*          Physical Maximum (0),           */
0xC0,                           /*      End Collection,                     */
0x09, 0x7F,                     /*      Usage (7Fh),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x17,                     /*          Report ID (23),                 */
0x09, 0x80,                     /*          Usage (80h),                    */
0x75, 0x10,                     /*          Report Size (16),               */
0x95, 0x01,                     /*          Report Count (1),               */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x27, 0xFF, 0xFF, 0x00, 0x00,   /*          Logical Maximum (65535),        */
0xB1, 0x02,                     /*          Feature (Variable),             */
0x09, 0x83,                     /*          Usage (83h),                    */
0x26, 0xFF, 0x00,               /*          Logical Maximum (40),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0xB1, 0x02,                     /*          Feature (Variable),             */
0x09, 0xA9,                     /*          Usage (A9h),                    */
0x09, 0xAA,                     /*          Usage (AAh),                    */
0x75, 0x01,                     /*          Report Size (1),                */
0x95, 0x02,                     /*          Report Count (2),               */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x25, 0x01,                     /*          Logical Maximum (1),            */
0xB1, 0x02,                     /*          Feature (Variable),             */
0x75, 0x06,                     /*          Report Size (6),                */
0x95, 0x01,                     /*          Report Count (1),               */
0xB1, 0x03,                     /*          Feature (Constant, Variable),   */
0xC0,                           /*      End Collection,                     */
0x09, 0x7D,                     /*      Usage (7Dh),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x19,                     /*          Report ID (25),                 */
0x09, 0x7E,                     /*          Usage (7Eh),                    */
0x26, 0xFF, 0x00,               /*          Logical Maximum (255),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0xC0,                           /*      End Collection,                     */
0x09, 0x74,                     /*      Usage (74h),                        */
0xA1, 0x02,                     /*      Collection (Logical),               */
0x85, 0x18,                     /*          Report ID (24),                 */
0x09, 0x22,                     /*          Usage (22h),                    */
0x15, 0x01,                     /*          Logical Minimum (1),            */
0x25, 0x28,                     /*          Logical Maximum (40),           */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (1),               */
0x91, 0x02,                     /*          Output (Variable),              */
0x09, 0x75,                     /*          Usage (75h),                    */
0x09, 0x76,                     /*          Usage (76h),                    */
0x15, 0x00,                     /*          Logical Minimum (0),            */
0x26, 0xFF, 0x00,               /*          Logical Maximum (255),          */
0x75, 0x08,                     /*          Report Size (8),                */
0x95, 0x01,                     /*          Report Count (2),               */
0x91, 0x02,                     /*          Output (Variable),              */
0xC0,                           /*      End Collection,                     */

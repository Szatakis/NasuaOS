1. Kernel
* [x]    Multitasking
* [x]    Multithreading
* [x]    Własny scheduler procesów
* [x]    Dynamiczne zarządzanie pamięcią
* [x]    Virtual Memory / Paging
* [x]    User Mode / Kernel Mode (ring 0 kernel, ring 3 dla wybranych aplikacji)
* [x]    Obsługa procesów i priorytetów
* [x]    System przerwań
* [x]    SMP – obsługa wielu rdzeni CPU
* [x]    Kernel Logger
* [x]    Kernel Panic Screen 
* [x]    Safe Mode
* [x]    System usług (Services)
* [x]    BootManager

2. GUI i pulpit
* [ ]    System powiadomień
* [ ]    Ikony pulpitu jeśli coś jest w folderze desktop danego użytkownika pojawia się na desktop i jeśli kliknę w plik odpala się w suaedit a jeśli folder to w file explorer a prawym klikiem menu typu delete new rename open open with properties 
3. Wbudowane aplikacje
* [x]    Kalkulator w ring 3 przez ograniczone API GUI
* [x]    Zdarzenia timerów GUI dla aplikacji ring 3
* [x]    Snake w ring 3 przez ograniczone API GUI i timery
* [x]    Wszystkie apklikacje do ring 3 brak mozliwosci utworzenia aplikacji bez ring 3

5. Sterowniki i sprzęt

* [ ]    USB 1.0
* [ ]    USB 1.1
* [ ]    USB 2.0
* [ ]    USB 3.0
* [ ]    USB HID – klawiatury i myszy
* [ ]    Sterowniki audio
* [ ]    AC’97
* [ ]    Intel HDA
* [ ]    PCI / PCIe
* [ ]    AHCI / SATA
* [ ]    NVMe
* [ ]    Virtio GPU
* [ ]    Virtio Network
* [ ]    Ethernet
* [ ]    DHCP
* [ ]    TCP/IP
* [ ]    DNS
* [ ]    IPv4 / IPv6

8. Bezpieczeństwo

* [ ]    System uprawnień
* [x]    Izolacja procesów (oddzielne przestrzenie adresowe dla procesów ring 3)
* [x]    Ochrona pamięci procesów (prawa stron user/kernel oraz NX)
* [x]    System ringów x86_64: ring 0 dla kernela, ring 3 dla wybranych aplikacji; ringi 1–2 nieużywane

10. Architektura i dodatkowe możliwości

* [ ]    Dodaj podczas kompilacji postemp np [1/120] czyli ktory z ilu krokow zrobiono np skompilowano 1 z 100 plikow
* [ ]    Wsparcie AArch64 i AArch32
* [ ]    Wsparcie RISC-V_32 i 64
* [ ]    Testy automatyczne
* [ ]    System debugowania
* [ ]    Recovery Mode
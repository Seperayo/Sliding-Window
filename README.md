# :school: Tugas Besar Jaringan Komputer :school:  <!-- omit in toc -->

# To do List <!-- omit in toc -->
- [x] **Refaktorisasi** recvfile
- [ ] **Refaktorisasi** sendfile
- [ ] Implementasi **ACK** % **packet loss**
- [ ] Implementasi **Checksum** corrupt data
- [ ] Implementasi **delay** + **timeout**

# :maple_leaf: Table of Content  <!-- omit in toc -->
- [:busts_in_silhouette: Nama Anggota Kelompok](#busts_in_silhouette-nama-anggota-kelompok)
- [:computer: Penjelasan Penggunaan Program](#computer-penjelasan-penggunaan-program)
	- [Compiling Program](#compiling-program)
- [Penjelasan Sliding Window](#penjelasan-sliding-window)
- [Penjelasan Fungsi-Fungsi](#penjelasan-fungsi-fungsi)
- [Pembagian Tugas](#pembagian-tugas)
- [About](#about)

## :busts_in_silhouette: Nama Anggota Kelompok
:point_right: **Seperayo** -  **13516068**

:point_right: **Hafizh Budiman** - **13516137**

:point_right: **Ilham Firdausi Putra** - **13516140**

## :computer: Penjelasan Penggunaan Program
### Compiling Program
Pragram dapat di compile dengan menggunakan perintah **`make`** pada command line. :ledger: Gunakan ***Linux*** demi kompatibilitas library yang lebih baik :ledger:

```
all: recvfile sendfile

recvfile: src/config.h src/config.cpp src/recvfile.cpp
	g++ -pthread src/config.cpp src/recvfile.cpp -o recvfile -std=c++11 

sendfile: src/config.h src/config.cpp src/sendfile.cpp
	g++ -pthread src/config.cpp src/sendfile.cpp -o sendfile -std=c++11 

clean: recvfile sendfile
	rm -f src/recvfile src/sendfile
```

## Penjelasan Sliding Window

## Penjelasan Fungsi-Fungsi
Berikut adalah fungsi-fungsi yang ada pada program kami

### config
1. `count_checksum`
---
2. `read_packet`
---
3. `create_ack`
---

### recvfile
1. `read_argument (recvfile)`
---
2. `prepare_connection (recvfile)`
---
3. `receive_file`
---

### sendfile

* get_packet_size
⋅⋅⋅Fungsi ini untuk menghitung ukuran dari packet yang akan dikirim.
---
* read_ack
⋅⋅⋅Fungsi untuk membaca ack yang diterima oleh pengirim, apakah dia sebuah NAK atau tidak, serta melakukan penghitungan checksum terhadap ACK yang diterima.
---
* get_ack
⋅⋅⋅Fungsi ini digunakan oleh sender untuk menerima ACK dan NAK. Fungsi ini dijalankan pada thread yang berbeda dengan thread untuk mengirim file, agar sender dapat mengirim packet dan menerima ACK/NAK ⋅⋅⋅secara bersamaan.
---
* read_argument (sendfile)
⋅⋅⋅Fungsi ini bertugas untuk melakukan parsing terhadap parameter input user.
---
* prepare_connection (sendfile)
⋅⋅⋅Fungsi ini digunakan untuk mempersiapkan socket, hostname, dan mempersiapkan file yang akan dikirimkan.
---
* send_file
⋅⋅⋅Fungsi ini digunakan untuk mengirimkan packet. Mutex lock diimplementasikan pada fungsi ini untuk menjaga sinkronisasi dari kedua thread.
---

## Pembagian Tugas
| Nama Anggota        | Tugas |
| ------------------- | ----- |
| Seperayo            |       |
| Ilham Firdaus Putra |       |
| Hafizh Budiman      |       |

## About
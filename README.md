# COMPTE RENDU – TP Linux Embarqué – VEEK-MT2S (DE10-Standard)

## Auteurs

* Adrien Lascar
* Pierre Mirouze

---

## 1. Prise en main

*(Cette section est implicite dans notre travail mais non traitée en détail dans ce compte rendu)*

---

## 2. Modules kernel (TP2)

### 2.1 Accès aux registres

Un programme en espace utilisateur a été développé pour accéder directement aux registres mémoire de la carte via `mmap()`, notamment à l’adresse `0xFF203000` (GPIO).
Cette méthode est rapide mais :

* Non portable (adresse physique fixe)
* Moins sécurisée (pas de contrôle noyau)
* Non maintenable à long terme (non conforme aux pratiques Linux)

---

### 2.2 Compilation de module noyau sur la VM

#### Question 1 – Création d’un module noyau simple

Un premier module simple a été créé, avec les fonctions `__init` et `__exit`, affichant des messages via `printk()` au chargement/déchargement :

```c
static int __init mon_module_init(void) {
    printk(KERN_INFO "Driver enregistre\n");
    return 0;
}
```

Ce module a été compilé avec les sources du noyau `linux-socfpga`, testé avec `insmod`, `rmmod` et vérifié via `dmesg`.

#### Question 2 – Enregistrement d’un driver caractère

Ajout d’un driver caractère avec `register_chrdev()` et une structure `file_operations`.
Les fonctions `.open` et `.release` affichent des messages pour valider leur bon fonctionnement.

---

### 2.3 Cross-compilation de modules noyau

#### 2.3.1 Préparation de la compilation

Récupération du fichier `/proc/config.gz` depuis la carte, décompression et renommage en `.config`. Commandes utilisées :

```bash
export CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf-
export ARCH=arm
make prepare
make scripts
```

Compilation ensuite avec `make`.

---

### 2.4 Chenillard

Un module chenillard utilisant un timer noyau (`mod_timer`) a été implémenté :

* **/proc/ensea/speed** : configurer la vitesse
* **/proc/ensea/dir** : changer le sens du chenillard
* **/dev/ensea\_leds** : lire/écrire le pattern courant

Le timer est basé sur `jiffies` et `msecs_to_jiffies()` pour le timing. Le pattern est écrit avec `iowrite32()`.

---

## 3. Device Tree (TP3)

### 3.1 Intégration du module dans le Device Tree

Ajout d’un nœud dans le fichier `.dts` :

```dts
ensea_leds@0xff200000 {
    compatible = "dev,ensea";
    reg = <0xff200000 0x10>;
};
```

Le module utilise `of_match_table` pour détecter ce nœud à l’initialisation. Compilation via `dtc`, puis copie du `.dtb` dans `/mntboot`.

---

### 3.2 Module accédant aux LEDs via /dev

Développement d’un platform driver contrôlant des LEDs déclarées dans le Device Tree. Mapping via :

```c
dev->regs = devm_ioremap_resource(&pdev->dev, r);
```

Interface utilisateur créée via `/dev/ensea_leds` avec les fonctions `.read` / `.write` / `.open` / `.release`.

---

### 3.3 Module final – Chenillard complet

Fonctionnalités finales respectées :

* **/proc/ensea/speed** : lecture de la vitesse
* **/proc/ensea/dir** : lecture/écriture du sens
* **/dev/ensea\_leds** : lecture/écriture du pattern
* Timer noyau utilisé pour l'animation du chenillard

---

## 4. Petit projet : Afficheurs 7 segments

### 4.1 Prise en main

Développement du module `7SEG.c` pour afficher une valeur fixe sur les afficheurs HEX. Utilisation de :

```c
iowrite32(szMask[i], hex_base + 4 * i);
```

L’adresse de base a été récupérée via `fpga.cpp`.

---

### 4.2 Affichage de l’heure (`7SEGHeure.c`)

Un module avec timer noyau affiche l’heure système (HHMMSS) chaque seconde, avec :

```c
ktime_get_real_ts64(&ts);
time_to_tm(ts.tv_sec, 0, &tm);
```

Le tout est affiché en miroir sur les afficheurs HEX.

#### Remarque

Une tentative de synchronisation avec une source RTC ou Internet n’a pas modifié l'heure noyau, uniquement celle de l’interface LXDE.

---

## Conclusion

Ce TP a permis :

* Écriture de modules noyau simples/complexes (timers, drivers)
* Communication avec l’espace utilisateur via `/dev`, `/proc`
* Cross-compilation de modules sur VM Linux
* Manipulation du Device Tree
* Accès matériel réel (LEDs, afficheurs 7 segments)
* Déploiement sur une vraie plateforme SoC FPGA (VEEK-MT2S)

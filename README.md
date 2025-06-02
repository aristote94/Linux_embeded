# COMPTE RENDU – TP Linux Embarqué – VEEK-MT2S (DE10-Standard)

## Auteurs

* Adrien Lascar
* Pierre Mirouze

---

## 1. Prise en main

*(Cette section est implicite dans notre travail mais non traitée en détail dans ce compte rendu)*
Cf. documentation TP Linux Embarqué pour les étapes de flash, configuration réseau, SSH, partitions, etc.&#x20;

---

## 2. Modules kernel (TP2)

### 2.1 Accès aux registres

Un programme utilisateur accède aux registres mémoire via `mmap()` à l’adresse `0xFF203000` pour piloter les GPIO (LEDs). Cette méthode est rapide mais :

* Non portable : dépend de l’adresse physique fixe
* Moins sécurisée : pas de supervision du noyau
* Moins maintenable : non conforme aux pratiques Linux modernes

---

### 2.2 Compilation de module noyau sur la VM

#### Question 1 – Création d’un module noyau simple

Le fichier `le_module.c` correspond à cette étape. Il enregistre un driver caractère simple :

```c
result = register_chrdev(MAJOR_NUM, "le_driver", &fops);
```

Les fonctions `.open` et `.release` déclenchent des messages dans le `dmesg`, validant le bon fonctionnement. Le module est chargé avec `insmod` et déchargé avec `rmmod`.

#### Question 2 – Enregistrement d’un driver caractère

`le_module.c` démontre une initialisation correcte de la structure `file_operations`, offrant un exemple minimal d’un module noyau interagissant via un fichier spécial `/dev`.

---

### 2.3 Cross-compilation de modules noyau

#### 2.3.1 Préparation de la compilation

Récupération de `/proc/config.gz` sur la carte, renommé en `.config`, suivi des commandes :

```bash
export CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf-
export ARCH=arm
make prepare
make scripts
```

Compilation croisée sur la VM fournie en TD.

---

### 2.4 Chenillard

Un chenillard complet est implémenté dans le fichier `gpio_leds.c`, qui va bien au-delà de l'exemple fourni par le professeur (`gpio-ledsProf.c`).

**Fonctionnalités :**

* Fichier `/dev/ensea_leds` : lit/écrit le motif affiché
* `/proc/ensea/speed` : lecture/écriture de la vitesse
* `/proc/ensea/dir` : lecture/écriture du sens
* Utilisation d’un timer noyau : `setup_timer` et `mod_timer`
* Mise à jour automatique via `jiffies`

Le comportement cyclique du pattern est mis à jour dynamiquement :

```c
pattern = ((pattern << 1) | (pattern >> (LED_COUNT - 1))) & ((1 << LED_COUNT) - 1);
```

Ce code montre une bonne maîtrise du développement de driver character Linux, intégrant accès matériel, gestion de timer, interface `/proc` et liaison avec le Device Tree.

---

## 3. Device Tree (TP3)

### 3.1 Intégration du module dans le Device Tree

Ajout d’un nœud dans `soc_system.dts` :

```dts
ensea_leds@0xff200000 {
    compatible = "dev,ensea";
    reg = <0xff200000 0x10>;
};
```

Ce nœud est détecté par le `platform_driver` grâce à la table :

```c
static struct of_device_id ensea_leds_dt_ids[] = {
    { .compatible = "dev,ensea" }, { }
};
```

Compilation via `dtc`, copie dans `/mntboot`, et redémarrage.

---

### 3.2 Module accédant aux LEDs via /dev

Le module `gpio_leds.c` utilise un `miscdevice` pour créer `/dev/ensea_leds`. L’accès mémoire aux registres LED est fait via :

```c
dev->regs = devm_ioremap_resource(&pdev->dev, r);
```

Les lectures et écritures interagissent avec le matériel et modifient `leds_value`, mis à jour dans les registres par `iowrite32()`.

---

### 3.3 Module final – Chenillard complet

Fonctionnalités finales respectées :

* `/proc/ensea/speed` : vitesse de balayage
* `/proc/ensea/dir` : direction
* `/dev/ensea_leds` : pattern binaire affiché
* Timer noyau pour défilement automatique

Le module respecte entièrement le cahier des charges défini dans le TP.

---

## 4. Petit projet : Afficheurs 7 segments

### 4.1 Prise en main

Le fichier `7SEG.c` initialise les afficheurs HEX et affiche les chiffres 1 à 6 via `szMask[]` :

```c
iowrite32(szMask[i + 1], hex_base + 4 * i);
```

L’adresse utilisée est `0xFF233000`, déterminée par analyse de `fpga.cpp` ;(projet Quartus).

### 4.2 Affichage de l’heure (`7SEGHeure.c`)

Un timer noyau met à jour l’heure système chaque seconde :

```c
ktime_get_real_ts64(&ts);
time_to_tm(ts.tv_sec, 0, &tm);
```

Les chiffres sont affichés dans le bon ordre (effet miroir) sur les 6 afficheurs HEX, sous le format HHMMSS.

Affichage inversé :

```c
for (i = 0; i < DIGIT_COUNT; i++) {
    iowrite32(szMask[digits[i]], hex_base + 4 * (DIGIT_COUNT - 1 - i));
}
```

Remarque : la tentative de synchronisation avec l’heure RTC ou Internet n’a pas modifié l’heure noyau, uniquement celle de l’environnement utilisateur LXDE. La seule option viable est le décalage de 2h dans le soft ;).

---

## Conclusion

Ce TP a permis de :

* Écrire et charger des modules noyau simples et complexes (timers, drivers)
* Interfacer noyau et espace utilisateur via `/dev` et `/proc`
* Cross-compiler des modules sur VM pour architecture ARM
* Utiliser et modifier le Device Tree
* Accéder au matériel réel (GPIO, afficheurs HEX)
* Déployer et tester sur une plateforme embarquée réelle (SoC FPGA VEEK-MT2S)

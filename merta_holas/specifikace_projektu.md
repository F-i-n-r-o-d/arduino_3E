# Rehabilitační LED tyč — popis systému

## Co to je

Dvě stejné LED tyče s akcelerometrem a WiFi a počítač.
Mentor na své tyči předvede cvik, pacient ho na své tyči zopakuje.
Obě tyče pošlou záznam pohybu do počítače, kde se porovnají a uloží.

Tyče mezi sebou nekomunikují. Veškeré vyhodnocení probíhá na PC.
Role mentora a pacienta přiděluje tyčím PC na začátku sezení.

---

## Průběh cvičení

**1. Připojení.** Obě tyče se připojí přes WiFi k PC a přiřadí se jim role.

**2. Mentor předvede cvik.** Spustí záznam a provede pohyb — například
naklonění tyče do určitého úhlu a zpět. Záznam se odešle na PC jako vzor.

**3. Pacient cvik zopakuje.** Provede stejný pohyb na své tyči.
Vzor z mentorovy tyče se mu nikde nezobrazuje — řídí se tím, co si
zapamatoval. Zpětná vazba přijde až po dokončení.

**4. Porovnání a uložení.** PC porovná oba záznamy, výsledek zobrazí
mentorovi a obojí uloží do historie.

---

## Co tyč zobrazuje

Během cvičení svítí na tyči kulička — několik sousedních diod tvořících
světelný bod. Její pozice odpovídá aktuálnímu náklonu tyče: jeden konec
tyče je jeden krajní úhel, druhý konec opačný.

Platí pro obě tyče stejně: **každý vidí jen svou vlastní kuličku.**

Mimo cvičení tyč světlem signalizuje stav — čekání na start, běžící
záznam, odesílání dat, hotovo.

---

## Co se měří

Akcelerometr měří náklon tyče vůči svislici. Pohyby, při kterých se
mění sklon tyče (zvedání, upažování, naklánění do stran), se měří
spolehlivě. Otáčení tyče kolem svislé osy akcelerometr nepozná, cviky
je proto potřeba volit tak, aby se při nich sklon měnil.

Na PC jde posloupnost vzorků — čas od začátku záznamu a úhel náklonu.
K záznamu patří datum, jméno pacienta, název cviku a pořadí opakování.

Tyč si data ukládá i do vlastní paměti a odesílá je průběžně, takže
krátký výpadek WiFi cvičení nepřeruší.

---

## Co PC vyhodnocuje

Z porovnání vzoru a pokusu lze vyčíst několik samostatných věcí.
Má smysl je sledovat odděleně, ne slučovat do jednoho čísla.

**Rozsah pohybu** — dosáhl pacient stejných krajních úhlů jako mentor,
nebo pohyb zkrátil? Klinicky obvykle nejdůležitější údaj.

**Tvar pohybu** — jel stejnou dráhou, nebo někde uhnul?

**Tempo** — stejně rychle, pomaleji, nebo rychleji? Pomalejší provedení
správného tvaru není totéž co špatný pohyb, proto se hodnotí zvlášť.

**Plynulost** — byl pohyb hladký, nebo trhaný?

**Opakovatelnost** — jak moc se od sebe liší jednotlivá opakování.
Jedno povedené opakování může být náhoda, pět stejných už ne.

---

## Historie

Všechna sezení se ukládají. Mentor tak vidí vývoj rozsahu pohybu v čase,
zda se zlepšuje plynulost a zda se zmenšuje rozdíl mezi opakováními.

Tahle část dává cvičení smysl i pro pacienta — vidí, že se něco mění,
i když to při jednom cvičení není poznat.

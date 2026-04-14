# HackAndSlash Combat System

> An Unreal Engine 5.7 third-person action combat prototype inspired by the fluid, cancelable combo flow of **Devil May Cry** and **Final Fantasy XVI**

---

## Showcase

### Combat Combos
![Combat Combo](gif/combat_combo.gif)

### Heavy Attacks
![Heavy Attacks](gif/heavy_attacks.gif)

### Air Combos
![Air Combo](gif/air_combo.gif)

### Lock-On System
![Lock-On](gif/lockon.gif)

### Homing Projectile
![Projectile](gif/projectile.gif)

### Enemy Pull
![Pull](gif/pull.gif)

### Style Rank System
![Style Rank](gif/style_rank.gif)

### Dodge System
![Dodge](gif/dodge.gif)

---

## Concept

Most third-person action games either go for grounded, weighty swings or fully stylized air-juggle combos. **This project** is me building out the second half from scratch, a responsive hack/slash sandbox where every attack feels like it can be canceled into the next one, movement never fights the animation, and the camera stays glued to the action.

The goal is a combat loop that feels **reactive, not scripted**. Inputs get buffered during the recovery window of the previous swing, attacks chain into each other without snapping back to idle, and movement locks out only while a strike is actually connecting, not for the whole montage.

---

## Systems

### Combat Component
`UHSCombatComponent` drives a combo state machine with light, heavy, and air attack arrays, a buffered-input system, and animation-notify callbacks for combo windows and damage frames. Sword traces run through `SphereTraceMulti` against `ECC_Pawn` and route damage through a `BlueprintNativeEvent` interface. Camera shake triggers on hit with separate intensity for light, heavy, and air attacks.

### Style Rank System
`UHSStyleComponent` tracks a DMC-style meter (D → C → B → A → S → SS → SSS) that rewards varied, aggressive play. Hits award points with a variety bonus for mixing attack types, dodges award bonus points, and taking damage deducts heavily. Higher ranks decay faster, pushing the player to stay aggressive. A combo counter tracks hit chains with a timeout reset.

### Lock-On System
DMC3/FF16-style lock-on with dynamic camera management. Camera focus uses a weighted midpoint between player and enemy with a fixed height (DMC3 pattern — never chases enemy Z position). Separate arm lengths for ground and air combat. Pitch clamping prevents the camera from going under the action during air combos. Lock-on reticle widget attaches to the target.

### Homing Projectile
FF16-style fire bolt bound to Q. Spawns from the left hand bone with a cast animation on the UpperBody slot. Homes toward locked target, auto-targets nearest enemy if not locked on, or fires straight forward if no enemy in range. Feeds the style meter on hit.

### Enemy Pull
DMC4 Nero Snatch-style pull ability that yanks enemies toward the player. Cancels active attacks on activation. Uses Mixamo grab animation on the UpperBody layer so legs keep moving.

### Air Combat
DMC-style air combos with progressive gravity per hit. First air hit nearly freezes gravity, each subsequent hit adds weight. Vertical velocity snaps to zero on each air attack for consistent hang time. Jump cancel allows extending air combos indefinitely.

### Hit Reactions
Enemies feature knockback physics (light/heavy/projectile with different forces), hitstop freeze frames, multiple randomized hit react montages for variety, and directional facing toward the attacker. Heavy hits get orange damage numbers, light hits get white.

### Damage Interface
`IHSDamageable` is a lightweight interface any hittable actor implements with both basic and extended damage calls (hit direction + attack weight for knockback/VFX).

### Animation Layering
`Layered Blend per Bone` in the ABP allows upper body montages (projectile cast, enemy pull) to play on the right arm while the lower body continues locomotion seamlessly.

---

## Controls

| Action | Key |
|--------|-----|
| Move | WASD |
| Look | Mouse |
| Sprint | Left Shift |
| Jump / Jump Cancel | Space |
| Light Attack | Left Mouse Button |
| Heavy Attack | Right Mouse Button |
| Dodge | Middle Mouse |
| Lock-On | Tab |
| Projectile | Q |
| Pull Enemy | E |

---

## Tech Stack

| Tool | Version |
|------|---------|
| Unreal Engine | 5.7 |
| Language | C++ (gameplay) + Blueprint (wiring) |
| Input | Enhanced Input System |
| Animation | Animation Blueprint + Blendspaces + Montages + Layered Blend per Bone |
| Damage | BlueprintNativeEvent Interface |
| VFX | Niagara (hit effects) |
| UI | UMG Widgets (Style HUD, Damage Numbers, Lock-On Reticle) |

---

## Project Structure

```
Source/HackSlashMovement/
├── Character/
│   ├── HSPlayerCharacter.h/.cpp
│   ├── HSPlayerAnimInstance.h/.cpp
│   └── HSDummyEnemy.h/.cpp
├── Combat/
│   ├── HSCombatComponent.h/.cpp
│   ├── HSStyleComponent.h/.cpp
│   ├── HSHomingProjectile.h/.cpp
│   ├── HSDamageable.h
│   ├── ANS_ComboWindow.h/.cpp
│   ├── AN_SwordTrace.h/.cpp
│   └── AN_AttackFinished.h/.cpp
├── UI/
│   ├── HSStyleHUD.h/.cpp
│   ├── HSDamageNumber.h/.cpp
│   ├── HSDamageNumberWidget.h/.cpp
│   └── HSLockOnReticle.h/.cpp
└── HackSlashMovement.Build.cs
```

---

## Roadmap

### Core Framework
- [x] Enhanced Input, movement, sprint, jump
- [x] Combo state machine with buffered input and sword trace
- [x] Damageable interface with extended hit info (direction + weight)
- [x] Dummy enemy with health, hit react, death cleanup

### Combat
- [x] Light combo chain (4-hit)
- [x] Heavy combo chain (4-hit)
- [x] Air combo system with progressive gravity
- [x] Attack rotation toward input direction
- [x] Jump cancel for infinite air combo tech
- [x] Camera shake on melee hit and projectile fire/impact
- [x] Hit reactions with knockback, hitstop, and randomized montages
- [x] Enemy pull ability (DMC4 Snatch style)
- [x] Homing projectile with left-hand spawn and cast animation
- [ ] Sprint attack
- [ ] Mixed combos (light into heavy launcher)
- [ ] Block / parry system

### Movement + Dodge
- [x] DMC/FF16 movement tuning
- [x] Phoenix Shift dodge system
- [x] Air dodge with configurable limit
- [x] Dodge cancels active attacks
- [x] Movement input cancels dodge recovery

### Lock-On + Camera
- [x] Lock-on targeting with dot product scoring
- [x] FF16-style camera offset and framing
- [x] DMC3-style fixed height camera (no vertical chase)
- [x] Dynamic arm length (ground vs air)
- [x] Pitch clamping to prevent under-action camera
- [x] Lock-on reticle widget on target

### Style System + HUD
- [x] Style rank meter (D → SSS) with decay and variety bonuses
- [x] Combo counter with timeout
- [x] Style HUD widget
- [x] Floating damage numbers (FF16 style, color-coded)
- [x] Lock-on reticle HUD

### Animation
- [x] Locomotion state machine with blendspaces
- [x] Upper body animation layering (Layered Blend per Bone)
- [x] Mixamo retargeted animations for pull and projectile cast
- [x] Velocity-based jump/fall transition

### Environment
- [x] PCG vegetation system
- [x] Rendering optimization (ray tracing disabled, cull distance volumes)

---

## Author

**Jake Fernandez**, [@JakeeUp](https://github.com/JakeeUp)
M.F.A. Game Programming, University of the Incarnate Word

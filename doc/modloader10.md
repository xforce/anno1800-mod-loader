# ModLoader 10 Changes

- Conditional ModOps
- Groups
- Disable warnings with AllowNoMatch
- Include Skip and Mod Relative Paths
- Copy Existing Nodes with Content
- &amp;gt; and &amp;lt; in Path
- Changed Merge Behavior
- Loading Order

## Conditional ModOps

You can use conditions to skip ModOps.
The ModOp will only be executed when nodes with the path in `Condition` are found - or none exist if you start with `!`.

```xml
<ModOp Type="addNextSibling" GUID="190886"
       Condition="!//Values[Standard/GUID='1500010714']">
  <Asset>
    <Template>ItemEffectTargetPool</Template>
    <Values>
      <Standard>
        <GUID>1500010714</GUID>
        <Name>all sand mines</Name>
      </Standard>
      <ItemEffectTargetPool>
        <EffectTargetGUIDs />
      </ItemEffectTargetPool>
    </Values>
  </Asset>
</ModOp>
```

Before you had to always add, remove the duplicate and deal with warnings.

```xml
  <ModOp Type="addNextSibling" GUID="190886">
    <Asset>
      <!-- 2nd duplicate to ensure there's always
           at least one to remove afterwards to avoid warnings -->
      <Template>ItemEffectTargetPool</Template>
      <Values>
        <Standard>
          <GUID>1500010714</GUID>
          <Name>duplicate</Name>
        </Standard>
      </Values>
    </Asset>
    <Asset>
      <Template>ItemEffectTargetPool</Template>
      <Values>
        <Standard>
          <GUID>1500010714</GUID>
          <Name>all sand mines</Name>
        </Standard>
        <ItemEffectTargetPool>
          <EffectTargetGUIDs />
        </ItemEffectTargetPool>
      </Values>
    </Asset>
  </ModOp>
  <!-- remove duplicates -->
  <ModOp Type="remove"
         Path="//Asset[Values/Standard/GUID='1500010714'][position() < last()]"/>
```

### Conditional Includes

Conditions also work for includes.

```xml
<Include File="/products/cheese/assets.include.xml"
         Condition="!//Values[Standard/GUID='1500010102']" />
```

## Groups

You can now also group your ModOps for combined `Skip` or `Condition` use.
They basically behave the same was as `Include` except you don't need an extra file.

```xml
<Group Condition="">
  <ModOp />
  <ModOp />
</Group>
```

## Disable warnings with `AllowNoMatch`

Disable match not found warnings.
Useful if you want to add items, but only if they don't exist.

Adding a product only once to a list with `AllowNoMatch`:
```xml
<ModOp Type="add" GUID="120055"
       Path="/Values/ProductStorageList/ProductList[not(Item/Product='1500010102')]"
       AllowNoMatch="1">
  <Item>
    <Product>1500010102</Product>
  </Item>
</ModOp>
```

The same without `AllowNoMatch` while dealing with warnings:
```xml
<ModOp Type="addNextSibling" GUID="120055">
  <Asset>
    <Template>fallback</Template>
    <Values>
      <Standard>
        <GUID>1500010221</GUID>
      </Standard>
      <ProductStorageList>
        <ProductList>
          <Item />
        </ProductList>
      </ProductStorageList>
    </Values>
  </Asset>
</ModOp>
<ModOp Type="add" GUID="120055"
       Path="/Values/ProductStorageList/ProductList[not(Item/Product='1500010102')] | //Values[Standard/GUID='1500010221']/ProductStorageList/ProductList">
  <Item>
    <Product>1500010102</Product>
  </Item>
</ModOp>
<ModOp Type="remove" GUID="1500010221"/>
```



## Include `Skip` and Mod Relative Paths

The `Skip` attribute is now also supported on `Include` operations.

And you don't need to use endless `../../` anymore if you want to point to top-level files in your mod.
If you start with `/` the path is treated as relative to your mod folder.

```xml
<ModOps>
  <Include File="/feature.include.xml" Skip="1" />
</ModOps>
```

Note: the skip happens when the attribute `Skip` is present.
It doesn't matter if you write `Skip="1"`, `Skip="True"` or even `Skip="0"` - all of them lead to skipping the include.

## Copy Existing Nodes with `Content`

```xml
<ModOp Type="replace" GUID="1500010225"
       Path="/Properties/Building/InfluencedVariationDirection"
       Content="//Values[Standard/GUID='1500010200']/Building/InfluencedVariationDirection" />
```

## &amp;gt; and &amp;lt; in Path

`<` and `>` are illegal characters in XML attributes.
While it works, you get annoying errors if you use XML validation.

Now you can work with escape sequences `&gt;` and `&lt;`:
```xml
<ModOp Type="remove" Path="//Asset[Values/Standard/GUID='1500010714'][position() &lt; last()]"/>
```

Works, but is not correct XML:
```xml
<ModOp Type="remove" Path="//Asset[Values/Standard/GUID='1500010714'][position() < last()]"/>
```

## Changed Merge Behavior

### Merge is order independent

The order of nodes do not matter anymore for successful merges.
Before you had to have the exact same order as the game.

```xml
<ModOp GUID="123" Type="merge" Path="/Values/Building">
  <AllowChangeDirection>1</AllowChangeDirection>
  <AllowChangeVariation>1</AllowChangeVariation>
</ModOp>
```

Result: 
```diff
<Asset>
  <Values>
    <Standard>
      <GUID>123</GUID>
    </Standard>
    <Building>
-     <AllowChangeVariation>0</AllowChangeVariation>
-     <AllowChangeDirection>0</AllowChangeDirection>
+     <AllowChangeVariation>1</AllowChangeVariation>
+     <AllowChangeDirection>1</AllowChangeDirection>
    </Building>
  </Values>
</Asset>
```

### Merge adds missing nodes

```xml
<ModOp GUID="123" Type="merge" Path="/Values/Building">
  <AllowChangeVariation>1</AllowChangeVariation>
  <BuildModeStartVariation>0</BuildModeStartVariation>
</ModOp>
```

Result: 
```diff
<Asset>
  <Values>
    <Standard>
      <GUID>123</GUID>
    </Standard>
    <Building>
-     <AllowChangeVariation>0</AllowChangeVariation>
+     <AllowChangeVariation>1</AllowChangeVariation>
+     <BuildModeStartVariation>0</BuildModeStartVariation>
    </Building>
  </Values>
</Asset>
```

### Merge top-level container like `replace`

If you have exactly one content node named the same as the last path element it will be skipped and it's content merged into the path node.

```xml
<ModOp GUID="123" Type="merge" Path="/Values/Building">
  <Building>
    <AllowChangeVariation>1</AllowChangeVariation>
  </Building>
</ModOp>
```

This behavior is kept to ensure backwards compatibility.
The recommended way is to omit that container node:

```xml
<ModOp GUID="123" Type="merge" Path="/Values/Building">
  <AllowChangeVariation>1</AllowChangeVariation>
</ModOp>
```

### Merge is strict

The following ModOp doesn't update `AllowChangeVariation` anymore.
Yes, it did before...
It will instead add the node to `Values`.

```xml
<ModOp GUID="123" Type="merge" Path="/Values">
  <AllowChangeVariation>1</AllowChangeVariation>
  <Building>
    <BuildModeStartVariation>0</BuildModeStartVariation>
  </Building>
</ModOp>
```

Result: 
```diff
<Asset>
  <Values>
    <Standard>
      <GUID>123</GUID>
    </Standard>
    <Building>
      <AllowChangeVariation>0</AllowChangeVariation>
+     <BuildModeStartVariation>0</BuildModeStartVariation>
    </Building>
+   <AllowChangeVariation>1</AllowChangeVariation>
  </Values>
</Asset>
```

### Merge doesn't work with same name nodes

Merging with multiple same name nodes (usually `Item`) is not supported anymore.

Relying on index is prone to compatibility issues.

```xml
<ModOp Type="merge" GUID="100780" Path="/Values/Maintenance">
  <Maintenances>
    <Item>
      <Product>1010017</Product>
      <Amount>50000</Amount>
      <InactiveAmount>30000</InactiveAmount>
    </Item>
    <Item>
      <Product>1010367</Product>
      <Amount>50</Amount>
    </Item>
  </Maintenances>
</ModOp>
```

Do individual merges instead:

```xml
<ModOp Type="merge" GUID="100780" Path="/Values/Maintenance/Maintenances/Item[Product='1010017']">
  <Product>1010017</Product>
  <Amount>50000</Amount>
  <InactiveAmount>30000</InactiveAmount>
</ModOp>
```

```xml
<ModOp Type="merge" GUID="100780" Path="/Values/Maintenance/Maintenances/Item[Product='1010367']">
  <Product>1010367</Product>
  <Amount>50</Amount>
</ModOp>
```

## Error on missing dependency

Mods mentioned in `ModDependencies` but not available and active in the `mods/` will print an error in the logs now.

## Loading order

You can specificy `LoadAfterIds` in `modinfo.json` now to load your mod after another.

No warning will printed if the mentioned mod is not available.
Use `ModDependencies` to signal required mods.

```json
{
  "ModID": "ModC",
  "LoadAfterIds": [
    "ModA",
    "ModB"
  ]
}
```

```json
{
  "ModID": "PostB",
  "LoadAfterIds": [
    "*",
    "PostA"
  ]
}
```

The order is as follows:
1. Mods with `LoadAfterIds` but without `*` following the order. Alphabetically order is ignored.
2. Mods without `LoadAfterIds` loaded alphabetically.
3. Mods with `*` loaded in order of `LoadAfterIds`.

Note: Thanks to step 2 the loading order without any `LoadAfterIds` is unchanged.

## Mods in sub-folders

```yaml
- mods/
  - Collection/
    - ModA/
      - data/
      - modinfo.json
    - ModB/
      - data/
      - modinfo.json
```

Collection folders do not act as mods.
They don't have activation or `ModID`s.

A mod manager may bulk enable/disable based on those folders though...

`modinfo.json` is required for mods in sub-folders.

## Mods in zip files

```yaml
- mods/
  - Mod.zip/
    - data/
    - modinfo.json
```

```yaml
- mods/
  - Collection.zip/
    - ModA/
      - data/
      - modinfo.json
    - ModB/
      - data/
      - modinfo.json
```

`modinfo.json` is required for mods in zips.

## Shared Mods within Mods

Perfect for shared data. You can version your shared data now to make sure the latest copy across all mods is used.

```yaml
- mods/
  - ModA/
    - data/
    - modinfo.json
    - SharedMod/
      - data/
      - modinfo.json
  - ModB/
    - data/
    - modinfo.json
    - SharedMod/
      - data/
      - modinfo.json
```

`modinfo.json` is required for such mods.

### Make part of your mod load at the end

Sub-mods also follow loading order.
You can use that fact to add a few ModOps to be executed after other mods, rather than loading your complete mod at the end.

## Activation via `activation.json` in mods folder

With enabling mods in zips an alternative way to deactivate mods is needed.

You can do that now with the following json file in your `mods/` folder:

```json
{
  "disabledIds": [
    "ModA",
    "ModB"
  ]
}
```

`ModID` from `modinfo.json` is used, and if not specified the folder name of the mod as a fallback.

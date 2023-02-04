# ModLoader 10 Changes

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

### Merge top-level node behavior

Merge throws a warning when the top-level node is the same as the selected path node.

This behavior is deprecated and should be fixed.
It still works to not break mods though.

```xml
<ModOp GUID="123" Type="merge" Path="/Values/Building">
  <Building>
    <AllowChangeVariation>1</AllowChangeVariation>
  </Building>
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

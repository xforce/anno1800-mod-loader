# ModOp Guide

## Simple ModOps

### Type `add`

```xml
<ModOp GUID="123" Type="add" Path="/Values">
  <Maintenance />
</ModOp>
```

Result: 
```diff
<Asset>
  <Values>
    <Standard>
      <GUID>123</GUID>
    </Standard>
    <Cost />
+   <Maintenance />
  </Values>
</Asset>
```

### Type `addNextSibling`

```xml
<ModOp GUID="123" Type="addNextSibling" Path="/Values/Standard">
  <Maintenance />
</ModOp>
```

Result: 
```diff
<Asset>
  <Values>
    <Standard>
      <GUID>123</GUID>
    </Standard>
+   <Maintenance />
    <Cost />
  </Values>
</Asset>
```

```xml
<ModOp GUID="123" Type="addNextSibling" Path="/Values/ConstructionCategory/BuildingList/Item[Building='1000178']">
  <Item>
    <Building>123</Building>
  </Item>
</ModOp>
```

Result: 
```diff
<Asset>
  <Values>
    <Standard>
      <GUID>123</GUID>
    </Standard>
    <ConstructionCategory>
      <BuildingList>
        <Item>
          <Building>1000178</Building>
        </Item>
+       <Item>
+         <Building>123</Building>
+       </Item>
        <Item>
          <Building>1010372</Building>
        </Item>
        <Item>
          <Building>1010343</Building>
        </Item>
      </BuildingList>
    </ConstructionCategory>
  </Values>
</Asset>
```

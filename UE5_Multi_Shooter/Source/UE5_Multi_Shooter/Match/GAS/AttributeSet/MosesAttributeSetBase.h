#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"

/**
 * Moses GAS Shared Helpers
 * - 모든 AttributeSet에서 공통으로 쓰는 매크로/헬퍼를 제공한다.
 * - Lyra 스타일: AttributeSet마다 반복되는 Getter/Setter/Attribute Getter 매크로를 통일한다.
 */
#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif

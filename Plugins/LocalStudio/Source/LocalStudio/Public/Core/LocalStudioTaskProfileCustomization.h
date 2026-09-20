#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

template <typename OptionType>
class SComboBox;

class LOCALSTUDIO_API FLocalStudioTaskProfileCustomization : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();

    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
    TSharedPtr<IPropertyHandle> ModelNameHandle;
    TArray<TSharedPtr<FString>> ModelOptions;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ModelComboBox;

    void RefreshModelOptions();
};
#endif
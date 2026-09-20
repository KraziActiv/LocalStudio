#if WITH_EDITOR

#include "Core/LocalStudioTaskProfileCustomization.h"
#include "Core/LocalStudioSettings.h"
#include "Core/OllamaManager.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<IPropertyTypeCustomization> FLocalStudioTaskProfileCustomization::MakeInstance()
{
    return MakeShared<FLocalStudioTaskProfileCustomization>();
}

void FLocalStudioTaskProfileCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    HeaderRow
        .NameContent()
        [
            PropertyHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            PropertyHandle->CreatePropertyValueWidget()
        ];
}

void FLocalStudioTaskProfileCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    ModelNameHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FOllamaTaskProfile, ModelName));
    TSharedPtr<IPropertyHandle> JobDescriptionHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FOllamaTaskProfile, JobDescription));

    RefreshModelOptions();

    StructBuilder.AddCustomRow(FText::FromString(TEXT("Model Name")))
        .NameContent()
        [
            ModelNameHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            SAssignNew(ModelComboBox, SComboBox<TSharedPtr<FString>>)
                .OptionsSource(&ModelOptions)
                .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
                return SNew(STextBlock)
                    .Text(FText::FromString(Item.IsValid() ? *Item : TEXT("")));
                    })
                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> SelectedItem, ESelectInfo::Type SelectInfo) {
                if (SelectedItem.IsValid() && ModelNameHandle.IsValid())
                {
                    ModelNameHandle->SetValue(*SelectedItem);
                }
                    })
                [
                    SNew(STextBlock)
                        .Text_Lambda([this]() -> FText {
                        FString CurrentValue;
                        if (ModelNameHandle.IsValid() && ModelNameHandle->GetValue(CurrentValue) == FPropertyAccess::Result::Success)
                        {
                            return CurrentValue.IsEmpty() ? FText::FromString(TEXT("Select Model...")) : FText::FromString(CurrentValue);
                        }
                        return FText::FromString(TEXT("Select Model..."));
                            })
                ]
        ];

    if (UOllamaManager* Manager = UOllamaManager::Get())
    {
        Manager->OnAvailableModelsRefreshed.AddLambda([this](const TArray<TSharedPtr<FString>>& NewModels) {
            RefreshModelOptions();
            if (ModelComboBox.IsValid())
            {
                ModelComboBox->RefreshOptions();
            }
            });
    }

    if (JobDescriptionHandle.IsValid())
    {
        StructBuilder.AddProperty(JobDescriptionHandle.ToSharedRef());
    }
}

void FLocalStudioTaskProfileCustomization::RefreshModelOptions()
{
    ModelOptions.Reset();

    if (UOllamaManager* Manager = UOllamaManager::Get())
    {
        ModelOptions = Manager->GetCachedModelDropdownOptions();
    }

    if (ModelOptions.Num() == 0)
    {
        ModelOptions.Add(MakeShared<FString>(TEXT("qwen2.5-coder:1.5b")));
        ModelOptions.Add(MakeShared<FString>(TEXT("qwen3-coder:30b")));
        ModelOptions.Add(MakeShared<FString>(TEXT("llava")));
    }
}

#endif
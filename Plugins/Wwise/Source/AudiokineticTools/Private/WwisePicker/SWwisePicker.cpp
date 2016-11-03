// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	SWwisePicker.cpp
------------------------------------------------------------------------------------*/

#include "AudiokineticToolsPrivatePCH.h"
#include "AkAudioDevice.h"
#include "WwisePicker/SWwisePicker.h"
#include "WwisePicker/WwiseEventDragDropOp.h"
#include "WwisePicker/WwiseWwuParser.h"
#include "SSearchBox.h"
#include "AkAudioBankGenerationHelpers.h"
#include "DirectoryWatcherModule.h"

#define LOCTEXT_NAMESPACE "AkAudio"

TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::EventIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::AuxBusIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::BusIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::FolderIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::PhysicalFolderIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::WorkUnitIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::ProjectIcon;
TSharedPtr<FSlateDynamicImageBrush> SWwisePicker::WwiseIcon;
const FName SWwisePicker::WwisePickerTabName = FName("WwisePicker");


SWwisePicker::SWwisePicker()
{
	FString ContentDir = FPaths::EnginePluginsDir() / TEXT("Wwise/Content/WwisePicker");
	FString EventIconPath = ContentDir / "event_nor.png";
	if (!FPaths::FileExists(EventIconPath))
	{
		ContentDir = FPaths::GamePluginsDir() / TEXT("Wwise/Content/WwisePicker");
		EventIconPath = ContentDir / "event_nor.png";
	}

	// Initialize icons
	FString AuxBusIconPath = ContentDir / "auxbus_nor.png";
	FString BusIconPath = ContentDir / "bus_nor.png";
	FString FolderIconPath = ContentDir / "folder_nor.png";
	FString PhysicalFolderIconPath = ContentDir / "physical_folder_nor.png";
	FString WorkUnitIconPath = ContentDir / "workunit_nor.png";
	FString ProjectIconPath = ContentDir / "wproj.png";
	FString WwiseIconPath = ContentDir / "wwise_icon_16.png";
	SWwisePicker::EventIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*EventIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::AuxBusIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*AuxBusIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::BusIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*BusIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::FolderIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*FolderIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::PhysicalFolderIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*PhysicalFolderIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::WorkUnitIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*WorkUnitIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::ProjectIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*ProjectIconPath), FVector2D(15.0f, 15.0f)));
	SWwisePicker::WwiseIcon = MakeShareable(new FSlateDynamicImageBrush(FName(*WwiseIconPath), FVector2D(16.0f, 16.0f)));
	AllowTreeViewDelegates = true;
}

void SWwisePicker::UpdateDirectoryWatcher()
{
	FString OldProjectFolder = ProjectFolder;
	ProjectFolder = FPaths::GetPath(WwiseBnkGenHelper::GetLinkedProjectPath());
	ProjectName = FPaths::GetCleanFilename(WwiseBnkGenHelper::GetLinkedProjectPath());

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	if (ProjectDirectoryModifiedDelegateHandle.IsValid())
	{
		DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(OldProjectFolder, ProjectDirectoryModifiedDelegateHandle);
	}

	DirectoryWatcherModule.Get()->RegisterDirectoryChangedCallback_Handle(
		ProjectFolder
		, IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &SWwisePicker::OnProjectDirectoryChanged)
		, ProjectDirectoryModifiedDelegateHandle
		);

}

void SWwisePicker::OnProjectDirectoryChanged(const TArray<struct FFileChangeData>& ChangedFiles)
{
	bool bFoundWorkUnit = false;
	for(auto File : ChangedFiles)
	{
		FPaths::NormalizeDirectoryName(File.Filename);
		if(File.Filename.EndsWith(FString(TEXT(".wwu"))) &&
			(File.Filename.Contains(TEXT("/Events/")) || File.Filename.Contains(TEXT("/Master-Mixer Hierarchy/"))) )
		{
			bFoundWorkUnit = true;
		}
	}

	if(bFoundWorkUnit)
	{
		OnPopulateClicked();
	}
}

SWwisePicker::~SWwisePicker()
{
	RootItems.Empty();
	SWwisePicker::EventIcon.Reset();
	SWwisePicker::AuxBusIcon.Reset();
	SWwisePicker::BusIcon.Reset();
	SWwisePicker::FolderIcon.Reset();
	SWwisePicker::PhysicalFolderIcon.Reset();
	SWwisePicker::WorkUnitIcon.Reset();
	SWwisePicker::ProjectIcon.Reset();
	SWwisePicker::WwiseIcon.Reset();

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	DirectoryWatcherModule.Get()->UnregisterDirectoryChangedCallback_Handle(ProjectFolder, ProjectDirectoryModifiedDelegateHandle);
}

void SWwisePicker::Construct(const FArguments& InArgs)
{
	UpdateDirectoryWatcher();
	SearchBoxFilter = MakeShareable( new StringFilter( StringFilter::FItemToStringArray::CreateSP( this, &SWwisePicker::PopulateSearchStrings ) ) );
	SearchBoxFilter->OnChanged().AddSP( this, &SWwisePicker::FilterUpdated );

    UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
    AkSettings->bRequestRefresh = false;
    
	ChildSlot
	[
		SNew(SBorder)
		.Padding(4)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)

			// Search
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 1, 0, 3)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					InArgs._SearchContent.Widget
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSearchBox)
					.HintText( LOCTEXT( "WwisePickerSearchTooltip", "Search Asset" ) )
					.OnTextChanged( this, &SWwisePicker::OnSearchBoxChanged )
					.SelectAllTextWhenFocused(false)
					.DelayChangeNotificationsWhileTyping(true)
				]
			]

			// Tree title
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3.0f)
				[
					SNew(SImage) 
					.Image(SWwisePicker::ProjectIcon.Get())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0,0,3,0)
				[
					SNew(STextBlock)
					.Font( FEditorStyle::GetFontStyle("ContentBrowser.SourceTitleFont") )
					.Text( this, &SWwisePicker::GetProjectName )
					.Visibility(InArgs._ShowTreeTitle ? EVisibility::Visible : EVisibility::Collapsed)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew( SSpacer )
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AkPickerPopulate", "Populate"))
					.OnClicked(this, &SWwisePicker::OnPopulateClicked)
				]
			]

			// Separator
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 1)
			[
				SNew(SSeparator)
				.Visibility( ( InArgs._ShowSeparator) ? EVisibility::Visible : EVisibility::Collapsed )
			]
			
			// Tree
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(TreeViewPtr, STreeView< TSharedPtr<FWwiseTreeItem> >)
				.TreeItemsSource(&RootItems)
				.OnGenerateRow( this, &SWwisePicker::GenerateRow )
				//.OnItemScrolledIntoView( this, &SPathView::TreeItemScrolledIntoView )
				.ItemHeight(18)
				.SelectionMode(InArgs._SelectionMode)
				.OnSelectionChanged(this, &SWwisePicker::TreeSelectionChanged)
				.OnExpansionChanged(this, &SWwisePicker::TreeExpansionChanged)
				.OnGetChildren( this, &SWwisePicker::GetChildrenForTree )
				//.OnSetExpansionRecursive( this, &SPathView::SetTreeItemExpansionRecursive )
				//.OnContextMenuOpening(this, &SPathView::MakePathViewContextMenu)
				.ClearSelectionOnClick(false)
			]
		]
	];
	OnPopulateClicked();
	TreeViewPtr->RequestTreeRefresh();
	ExpandFirstLevel();
}

void SWwisePicker::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
    UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
    if(AkSettings->bRequestRefresh)
    {
        ForceRefresh();
        AkSettings->bRequestRefresh = false;
    }
}

void SWwisePicker::ForceRefresh()
{
	FWwiseWwuParser::ForcePopulate();
	UpdateDirectoryWatcher();
	ConstructTree();
}

FText SWwisePicker::GetProjectName() const
{
	return FText::FromString(ProjectName);;
}

FReply SWwisePicker::OnPopulateClicked()
{
	FWwiseWwuParser::Populate();

	ConstructTree();
	
	return FReply::Handled();
}

void SWwisePicker::ConstructTree()
{
	RootItems.Empty(EWwiseTreeItemType::NUM_DRAGGABLE_WWISE_ITEMS);
	EWwiseTreeItemType::Type CurrentType = EWwiseTreeItemType::Event;
	while (CurrentType < EWwiseTreeItemType::NUM_DRAGGABLE_WWISE_ITEMS)
	{
		TSharedPtr<FWwiseTreeItem> NewRoot = FWwiseWwuParser::GetTree(SearchBoxFilter, RootItems.Num() > CurrentType ? RootItems[CurrentType] : nullptr, CurrentType);
		RootItems.Add(NewRoot);
		CurrentType = (EWwiseTreeItemType::Type)(((int)CurrentType) + 1);
	}
	RestoreTreeExpansion(RootItems);
	TreeViewPtr->RequestTreeRefresh();
}

void SWwisePicker::ExpandFirstLevel()
{
	// Expand root items and first-level work units.
	for(int32 i = 0; i < RootItems.Num(); i++)
	{
		TreeViewPtr->SetItemExpansion(RootItems[i], true);
	}
}

void SWwisePicker::ExpandParents(TSharedPtr<FWwiseTreeItem> Item)
{
	if(Item->Parent.IsValid())
	{
		ExpandParents(Item->Parent.Pin());
		TreeViewPtr->SetItemExpansion(Item->Parent.Pin(), true);
	}
}

FSlateBrush* GetIcon(EWwiseTreeItemType::Type ItemType)
{
	switch(ItemType)
	{
	case EWwiseTreeItemType::Event:
		return SWwisePicker::EventIcon.Get();
	case EWwiseTreeItemType::AuxBus:
		return SWwisePicker::AuxBusIcon.Get();
	case EWwiseTreeItemType::Bus:
		return SWwisePicker::BusIcon.Get();
	case EWwiseTreeItemType::Folder:
		return SWwisePicker::FolderIcon.Get();
	case EWwiseTreeItemType::PhysicalFolder:
		return SWwisePicker::PhysicalFolderIcon.Get();
	case EWwiseTreeItemType::StandaloneWorkUnit:
	case EWwiseTreeItemType::NestedWorkUnit:
		return SWwisePicker::WorkUnitIcon.Get();
	default:
		return NULL;
	}
}

TSharedRef<ITableRow> SWwisePicker::GenerateRow( TSharedPtr<FWwiseTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());

	EVisibility RowVisibility = TreeItem->IsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	//EVisibility RowVisibility = EVisibility::Visible;

	TSharedPtr<ITableRow> NewRow = SNew( STableRow< TSharedPtr<FWwiseTreeItem> >, OwnerTable )
		.OnDragDetected( this, &SWwisePicker::OnDragDetected )
		.Visibility(RowVisibility)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 1, 2, 1)
			.VAlign(VAlign_Center)
			[
				SNew(SImage) 
				.Image(GetIcon(TreeItem->ItemType))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TreeItem->DisplayName))
				.HighlightText(this, &SWwisePicker::GetHighlightText)
			]
		];

	TreeItem->TreeRow = NewRow.Get();

	return NewRow.ToSharedRef();
}

void SWwisePicker::GetChildrenForTree( TSharedPtr< FWwiseTreeItem > TreeItem, TArray< TSharedPtr<FWwiseTreeItem> >& OutChildren )
{
	OutChildren = TreeItem->Children;
}

FReply SWwisePicker::OnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
	{
		TArray<TSharedPtr<FWwiseTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();
		return FReply::Handled().BeginDragDrop(FWwiseEventDragDropOp::New(SelectedItems));
	}

	return FReply::Unhandled();
}

void SWwisePicker::PopulateSearchStrings( const FString& FolderName, OUT TArray< FString >& OutSearchStrings ) const
{
	OutSearchStrings.Add( FolderName );
}

void SWwisePicker::OnSearchBoxChanged( const FText& InSearchText )
{
	SearchBoxFilter->SetRawFilterText( InSearchText );
}

FText SWwisePicker::GetHighlightText() const
{
	return SearchBoxFilter->GetRawFilterText();
}

void SWwisePicker::FilterUpdated()
{

	FScopedSlowTask SlowTask(2.f, LOCTEXT("AK_PopulatingPicker", "Populating Wwise Picker..."));
	SlowTask.MakeDialog();
	for(int32 i = 0; i < RootItems.Num(); i++)
	{
		ApplyFilter(RootItems[i]);
	}
	TreeViewPtr->RequestTreeRefresh();
}

void SWwisePicker::SetItemVisibility(TSharedPtr<FWwiseTreeItem> Item, bool IsVisible)
{
	if( Item.IsValid() )
	{
		if( IsVisible )
		{
			// Propagate visibility to parents.
			SetItemVisibility(Item->Parent.Pin(), IsVisible);
		}
		Item->IsVisible = IsVisible;
		if( Item->TreeRow != NULL )
		{
			TSharedRef<SWidget> wid = Item->TreeRow->AsWidget();
			Item->TreeRow->AsWidget()->SetVisibility(IsVisible ? EVisibility::Visible : EVisibility::Collapsed);
		}
	}
}

void SWwisePicker::ApplyFilter(TSharedPtr<FWwiseTreeItem> ItemToFilter)
{

	EWwiseTreeItemType::Type CurrentType = EWwiseTreeItemType::Event;
	while (CurrentType < EWwiseTreeItemType::NUM_DRAGGABLE_WWISE_ITEMS)
	{
		TSharedPtr<FWwiseTreeItem> NewRoot = FWwiseWwuParser::GetTree(SearchBoxFilter, RootItems[CurrentType], CurrentType);
		RootItems[CurrentType] = NewRoot;
		CurrentType = (EWwiseTreeItemType::Type)(((int)CurrentType) + 1);
	}

	AllowTreeViewDelegates = false;
	RestoreTreeExpansion(RootItems);
	AllowTreeViewDelegates = true;
}

void SWwisePicker::RestoreTreeExpansion(const TArray< TSharedPtr<FWwiseTreeItem> >& Items)
{
	for(int i = 0; i < Items.Num(); i++)
	{
		if( LastExpandedPaths.Contains(Items[i]->FolderPath) )
		{
			TreeViewPtr->SetItemExpansion(Items[i], true);
		}
		RestoreTreeExpansion(Items[i]->Children);
	}
}

void SWwisePicker::TreeSelectionChanged( TSharedPtr< FWwiseTreeItem > TreeItem, ESelectInfo::Type /*SelectInfo*/ )
{
	if( AllowTreeViewDelegates )
	{
		const TArray<TSharedPtr<FWwiseTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();

		LastSelectedPaths.Empty();
		for (int32 ItemIdx = 0; ItemIdx < SelectedItems.Num(); ++ItemIdx)
		{
			const TSharedPtr<FWwiseTreeItem> Item = SelectedItems[ItemIdx];
			if ( Item.IsValid() )
			{
				LastSelectedPaths.Add(Item->FolderPath);
			}
		}
	}
}

void SWwisePicker::TreeExpansionChanged( TSharedPtr< FWwiseTreeItem > TreeItem, bool bIsExpanded )
{
	if( AllowTreeViewDelegates )
	{
		TSet<TSharedPtr<FWwiseTreeItem>> ExpandedItemSet;
		TreeViewPtr->GetExpandedItems(ExpandedItemSet);

		LastExpandedPaths.Empty();
		for (auto ExpandedItemIt = ExpandedItemSet.CreateConstIterator(); ExpandedItemIt; ++ExpandedItemIt)
		{
			const TSharedPtr<FWwiseTreeItem> Item = *ExpandedItemIt;
			if ( Item.IsValid() )
			{
				// Keep track of the last paths that we broadcasted for expansion reasons when filtering
				LastExpandedPaths.Add(Item->FolderPath);
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE